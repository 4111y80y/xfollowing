#include "MainWindow.h"
#include "BrowserWidget.h"
#include "KeywordPanel.h"
#include "PostListPanel.h"
#include "App/CefApp.h"
#include "Data/DataStorage.h"
#include "Core/PostMonitor.h"
#include "Core/AutoFollower.h"
#include <QCloseEvent>
#include <QSettings>
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStatusBar>
#include <QSet>
#include <QHeaderView>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mainSplitter(nullptr)
    , m_searchBrowser(nullptr)
    , m_centerPanel(nullptr)
    , m_keywordPanel(nullptr)
    , m_postListPanel(nullptr)
    , m_hideFollowedCheckBox(nullptr)
    , m_cooldownSpinBox(nullptr)
    , m_rightPanel(nullptr)
    , m_cooldownLabel(nullptr)
    , m_userBrowser(nullptr)
    , m_statusLabel(nullptr)
    , m_dataStorage(nullptr)
    , m_postMonitor(nullptr)
    , m_autoFollower(nullptr)
    , m_cefTimerId(0)
    , m_searchBrowserInitialized(false)
    , m_userBrowserInitialized(false)
    , m_cooldownTimer(nullptr)
    , m_cooldownSeconds(30)
    , m_remainingCooldown(0)
    , m_isCooldownActive(false) {

    setWindowTitle("X互关宝 - X.com互关粉丝助手");
    resize(1600, 900);

    // 初始化冷却计时器
    m_cooldownTimer = new QTimer(this);
    connect(m_cooldownTimer, &QTimer::timeout, this, &MainWindow::onCooldownTick);

    // 初始化数据存储
    m_dataStorage = new DataStorage(this);

    // 加载数据
    m_keywords = m_dataStorage->loadKeywords();
    m_posts = m_dataStorage->loadPosts();

    // 对加载的帖子进行去重（按作者去重）
    QList<Post> uniquePosts;
    QSet<QString> seenAuthors;
    for (const auto& post : m_posts) {
        if (!seenAuthors.contains(post.authorHandle)) {
            seenAuthors.insert(post.authorHandle);
            uniquePosts.append(post);
        }
    }
    m_posts = uniquePosts;

    // 添加固定的作者帖子（永久显示，不会隐藏或删除）
    addPinnedAuthorPost();

    // 初始化帖子监控器
    m_postMonitor = new PostMonitor(this);

    // 初始化自动关注器
    m_autoFollower = new AutoFollower(this);

    setupUI();
    setupConnections();
    loadSettings();

    // Start CEF message loop timer
    m_cefTimerId = startTimer(10);
}

MainWindow::~MainWindow() {
    saveSettings();
    if (m_cefTimerId) {
        killTimer(m_cefTimerId);
    }
}

void MainWindow::setupUI() {
    // Main splitter - 三栏布局
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);

    // 左侧 - 搜索浏览器
    m_searchBrowser = new BrowserWidget(m_mainSplitter);
    m_searchBrowser->setMinimumWidth(500);

    // 中间 - 控制面板
    m_centerPanel = new QWidget(m_mainSplitter);
    QVBoxLayout* centerLayout = new QVBoxLayout(m_centerPanel);
    centerLayout->setContentsMargins(10, 10, 10, 10);
    centerLayout->setSpacing(10);

    // 关键词设置区域
    m_keywordPanel = new KeywordPanel(m_centerPanel);
    m_keywordPanel->setKeywords(m_keywords);
    centerLayout->addWidget(m_keywordPanel);

    // Tab切换区域
    m_tabWidget = new QTabWidget(m_centerPanel);

    // Tab1: 帖子列表
    QWidget* postTab = new QWidget();
    QVBoxLayout* postLayout = new QVBoxLayout(postTab);
    postLayout->setContentsMargins(0, 0, 0, 0);

    m_postListPanel = new PostListPanel(postTab);
    m_postListPanel->setPosts(m_posts);
    postLayout->addWidget(m_postListPanel);

    // 隐藏已关注开关
    m_hideFollowedCheckBox = new QCheckBox("隐藏已关注的帖子", postTab);
    postLayout->addWidget(m_hideFollowedCheckBox);

    m_tabWidget->addTab(postTab, "监控帖子");

    // Tab2: 已关注作者列表
    QWidget* followedTab = new QWidget();
    QVBoxLayout* followedLayout = new QVBoxLayout(followedTab);
    followedLayout->setContentsMargins(0, 0, 0, 0);

    m_followedAuthorsTable = new QTableWidget(followedTab);
    m_followedAuthorsTable->setColumnCount(3);
    m_followedAuthorsTable->setHorizontalHeaderLabels({"作者", "关注时间", "来源帖子"});
    m_followedAuthorsTable->horizontalHeader()->setStretchLastSection(true);
    m_followedAuthorsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_followedAuthorsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_followedAuthorsTable->setAlternatingRowColors(true);
    m_followedAuthorsTable->setColumnWidth(0, 120);
    m_followedAuthorsTable->setColumnWidth(1, 100);
    followedLayout->addWidget(m_followedAuthorsTable);

    QLabel* hintLabel = new QLabel("双击作者可打开其主页(不自动关注)", followedTab);
    hintLabel->setStyleSheet("color: gray; font-size: 11px;");
    followedLayout->addWidget(hintLabel);

    m_tabWidget->addTab(followedTab, "已关注");

    centerLayout->addWidget(m_tabWidget, 1);

    // 冷却时间设置
    QHBoxLayout* cooldownLayout = new QHBoxLayout();
    QLabel* cooldownSettingLabel = new QLabel("关注冷却时间(秒):", m_centerPanel);
    m_cooldownSpinBox = new QSpinBox(m_centerPanel);
    m_cooldownSpinBox->setRange(30, 300);
    m_cooldownSpinBox->setValue(m_cooldownSeconds);
    m_cooldownSpinBox->setSuffix(" 秒");
    cooldownLayout->addWidget(cooldownSettingLabel);
    cooldownLayout->addWidget(m_cooldownSpinBox);
    cooldownLayout->addStretch();
    centerLayout->addLayout(cooldownLayout);

    // 更新已关注作者表格
    updateFollowedAuthorsTable();

    // 右侧 - 用户页浏览器（带倒计时提示）
    m_rightPanel = new QWidget(m_mainSplitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // 倒计时提示标签
    m_cooldownLabel = new QLabel("", m_rightPanel);
    m_cooldownLabel->setAlignment(Qt::AlignCenter);
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #ff6b6b; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setVisible(false);
    rightLayout->addWidget(m_cooldownLabel);

    // 用户浏览器
    m_userBrowser = new BrowserWidget(m_rightPanel);
    m_userBrowser->setMinimumWidth(500);
    rightLayout->addWidget(m_userBrowser, 1);

    // 设置分栏比例
    m_mainSplitter->setSizes({500, 350, 500});

    // 状态栏
    m_statusLabel = new QLabel("状态: 就绪");
    statusBar()->addWidget(m_statusLabel, 1);

    updateStatusBar();
}

void MainWindow::setupConnections() {
    // 搜索浏览器信号
    connect(m_searchBrowser, &BrowserWidget::browserCreated, this, &MainWindow::onSearchBrowserCreated);
    connect(m_searchBrowser, &BrowserWidget::loadFinished, this, &MainWindow::onSearchLoadFinished);
    connect(m_searchBrowser, &BrowserWidget::newPostsFound, this, &MainWindow::onNewPostsFound);

    // 用户浏览器信号
    connect(m_userBrowser, &BrowserWidget::browserCreated, this, &MainWindow::onUserBrowserCreated);
    connect(m_userBrowser, &BrowserWidget::loadFinished, this, &MainWindow::onUserLoadFinished);
    connect(m_userBrowser, &BrowserWidget::followSuccess, this, &MainWindow::onFollowSuccess);
    connect(m_userBrowser, &BrowserWidget::alreadyFollowing, this, &MainWindow::onAlreadyFollowing);
    connect(m_userBrowser, &BrowserWidget::followFailed, this, &MainWindow::onFollowFailed);

    // 帖子列表点击
    connect(m_postListPanel, &PostListPanel::postClicked, this, &MainWindow::onPostClicked);

    // 隐藏已关注开关
    connect(m_hideFollowedCheckBox, &QCheckBox::toggled, this, &MainWindow::onHideFollowedChanged);

    // 关键词变化
    connect(m_keywordPanel, &KeywordPanel::keywordsChanged, this, &MainWindow::onKeywordsChanged);

    // 已关注作者双击
    connect(m_followedAuthorsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onFollowedAuthorDoubleClicked);
}

void MainWindow::loadSettings() {
    QSettings settings("xfollowing", "X互关宝");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    if (settings.contains("splitterSizes")) {
        m_mainSplitter->restoreState(settings.value("splitterSizes").toByteArray());
    }

    m_hideFollowedCheckBox->setChecked(settings.value("hideFollowed", false).toBool());
}

void MainWindow::saveSettings() {
    QSettings settings("xfollowing", "X互关宝");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("splitterSizes", m_mainSplitter->saveState());
    settings.setValue("hideFollowed", m_hideFollowedCheckBox->isChecked());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();

    // 保存数据
    m_dataStorage->savePosts(m_posts);
    m_dataStorage->saveKeywords(m_keywords);

    if (m_searchBrowser) {
        m_searchBrowser->CloseBrowser();
    }
    if (m_userBrowser) {
        m_userBrowser->CloseBrowser();
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::timerEvent(QTimerEvent* event) {
    if (event->timerId() == m_cefTimerId) {
        CefHelper::DoMessageLoopWork();
    }
    QMainWindow::timerEvent(event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    // 只创建左侧搜索浏览器，右侧浏览器在点击帖子时才创建
    if (!m_searchBrowserInitialized && m_searchBrowser) {
        m_searchBrowserInitialized = true;
        QString profilePath = m_dataStorage->getProfilePath();
        qDebug() << "[INFO] Creating search browser with profile:" << profilePath;
        m_searchBrowser->CreateBrowserWithProfile("https://x.com/search?q=%E4%BA%92%E5%85%B3&f=live", profilePath);
    }
}

void MainWindow::onSearchBrowserCreated() {
    qDebug() << "[INFO] Search browser created";
    m_statusLabel->setText("状态: 搜索浏览器就绪，请登录X.com");
}

void MainWindow::onUserBrowserCreated() {
    qDebug() << "[INFO] User browser created";
}

void MainWindow::onSearchLoadFinished(bool success) {
    if (success) {
        qDebug() << "[INFO] Search page loaded";
        m_statusLabel->setText("状态: 搜索页面加载完成，开始监控帖子...");

        // 注入监控脚本
        injectMonitorScript();
    } else {
        m_statusLabel->setText("状态: 搜索页面加载失败");
    }
}

void MainWindow::onUserLoadFinished(bool success) {
    if (success && !m_currentFollowingHandle.isEmpty()) {
        qDebug() << "[INFO] User page loaded, executing follow script for:" << m_currentFollowingHandle;
        m_statusLabel->setText(QString("状态: 正在关注 @%1...").arg(m_currentFollowingHandle));

        // 执行自动关注脚本
        QString script = m_autoFollower->getFollowScript();
        m_userBrowser->ExecuteJavaScript(script);
    }
}

void MainWindow::onPostClicked(const Post& post) {
    qDebug() << "[INFO] Post clicked:" << post.authorHandle;

    // 检查是否在冷却中
    if (m_isCooldownActive) {
        m_statusLabel->setText(QString("状态: 冷却中，请等待 %1 秒后再关注").arg(m_remainingCooldown));
        return;
    }

    if (post.isFollowed) {
        m_statusLabel->setText(QString("状态: @%1 已关注").arg(post.authorHandle));
        return;
    }

    m_currentFollowingHandle = post.authorHandle;
    m_statusLabel->setText(QString("状态: 正在打开 @%1 的主页...").arg(post.authorHandle));

    // 首次点击时初始化右侧浏览器
    if (!m_userBrowserInitialized && m_userBrowser) {
        m_userBrowserInitialized = true;
        QString profilePath = m_dataStorage->getProfilePath();
        QString userUrl = QString("https://x.com/%1").arg(post.authorHandle);
        qDebug() << "[INFO] Creating user browser with profile:" << profilePath;
        m_userBrowser->CreateBrowserWithProfile(userUrl, profilePath);
    } else {
        // 浏览器已初始化，直接加载URL
        QString userUrl = QString("https://x.com/%1").arg(post.authorHandle);
        m_userBrowser->LoadUrl(userUrl);
    }
}

void MainWindow::onNewPostsFound(const QString& jsonData) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (!doc.isArray()) {
        return;
    }

    QJsonArray arr = doc.array();
    int newCount = 0;

    for (const auto& v : arr) {
        QJsonObject obj = v.toObject();
        Post post;
        post.postId = obj["postId"].toString();
        post.authorName = obj["authorName"].toString();
        post.authorHandle = obj["authorHandle"].toString();
        post.authorUrl = obj["authorUrl"].toString();
        post.content = obj["content"].toString();
        post.postUrl = obj["postUrl"].toString();
        post.matchedKeyword = obj["matchedKeyword"].toString();
        post.collectTime = QDateTime::currentDateTime();

        // 跳过无效数据
        if (post.authorHandle.isEmpty() || post.postId.isEmpty()) {
            continue;
        }

        // 去重：按作者去重（同一作者只保留一条帖子，因为目的是关注用户）
        bool exists = false;
        for (const auto& p : m_posts) {
            // 按postId去重，或者按作者去重
            if (p.postId == post.postId || p.authorHandle == post.authorHandle) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            m_posts.prepend(post);
            m_dataStorage->addPost(post);
            newCount++;
        }
    }

    if (newCount > 0) {
        m_postListPanel->setPosts(m_posts);
        updateStatusBar();
        qDebug() << "[INFO] Found" << newCount << "new posts";
    }
}

void MainWindow::onFollowSuccess(const QString& userHandle) {
    qDebug() << "[SUCCESS] Followed:" << userHandle;

    // 更新帖子状态
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts[i].authorHandle == m_currentFollowingHandle) {
            m_posts[i].isFollowed = true;
            m_posts[i].followTime = QDateTime::currentDateTime();
            m_dataStorage->updatePost(m_posts[i]);
        }
    }

    m_postListPanel->setPosts(m_posts);
    updateFollowedAuthorsTable();
    updateStatusBar();

    m_statusLabel->setText(QString("状态: 成功关注 @%1").arg(m_currentFollowingHandle));
    m_currentFollowingHandle.clear();

    // 启动冷却
    startCooldown();
}

void MainWindow::onAlreadyFollowing(const QString& userHandle) {
    qDebug() << "[INFO] Already following:" << userHandle;

    // 更新帖子状态
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts[i].authorHandle == m_currentFollowingHandle) {
            m_posts[i].isFollowed = true;
            m_dataStorage->updatePost(m_posts[i]);
        }
    }

    m_postListPanel->setPosts(m_posts);
    updateStatusBar();

    m_statusLabel->setText(QString("状态: @%1 已经关注过了").arg(m_currentFollowingHandle));
    m_currentFollowingHandle.clear();
}

void MainWindow::onFollowFailed(const QString& userHandle) {
    qDebug() << "[ERROR] Follow failed:" << userHandle;
    m_statusLabel->setText(QString("状态: 关注 @%1 失败").arg(m_currentFollowingHandle));
    m_currentFollowingHandle.clear();
}

void MainWindow::onHideFollowedChanged(bool checked) {
    m_postListPanel->setHideFollowed(checked);
}

void MainWindow::onKeywordsChanged() {
    m_keywords = m_keywordPanel->getKeywords();
    m_dataStorage->saveKeywords(m_keywords);

    // 重新注入监控脚本
    injectMonitorScript();
}

void MainWindow::updateStatusBar() {
    int total = m_posts.size();
    int followed = 0;
    int pending = 0;

    for (const auto& post : m_posts) {
        if (post.isFollowed) {
            followed++;
        } else {
            pending++;
        }
    }

    QString status = QString("已采集: %1 | 已关注: %2 | 待关注: %3").arg(total).arg(followed).arg(pending);
    statusBar()->showMessage(status);
}

void MainWindow::injectMonitorScript() {
    QString script = m_postMonitor->getMonitorScript(m_keywords);
    m_searchBrowser->ExecuteJavaScript(script);
    qDebug() << "[INFO] Monitor script injected";
}

void MainWindow::addPinnedAuthorPost() {
    // 固定的作者帖子（永久显示，不会隐藏或删除）
    const QString pinnedPostId = "2012906900250378388";
    const QString pinnedAuthorHandle = "4111y80y";

    // 检查是否已存在
    for (const auto& post : m_posts) {
        if (post.postId == pinnedPostId || post.authorHandle == pinnedAuthorHandle) {
            return;  // 已存在，不重复添加
        }
    }

    // 创建固定帖子
    Post pinnedPost;
    pinnedPost.postId = pinnedPostId;
    pinnedPost.authorHandle = pinnedAuthorHandle;
    pinnedPost.authorName = "X互关宝作者";
    pinnedPost.authorUrl = "https://x.com/" + pinnedAuthorHandle;
    pinnedPost.content = "[固定] X互关宝作者 - 欢迎互关交流!";
    pinnedPost.postUrl = "https://x.com/" + pinnedAuthorHandle + "/status/" + pinnedPostId;
    pinnedPost.matchedKeyword = "互关";
    pinnedPost.collectTime = QDateTime::currentDateTime();
    pinnedPost.isFollowed = false;

    // 添加到列表开头
    m_posts.prepend(pinnedPost);
}

void MainWindow::startCooldown() {
    // 获取用户设置的冷却时间
    m_cooldownSeconds = m_cooldownSpinBox->value();
    m_remainingCooldown = m_cooldownSeconds;
    m_isCooldownActive = true;

    // 禁用帖子列表点击
    m_postListPanel->setEnabled(false);

    // 显示倒计时
    updateCooldownDisplay();
    m_cooldownLabel->setVisible(true);

    // 启动计时器（每秒触发一次）
    m_cooldownTimer->start(1000);

    qDebug() << "[INFO] Cooldown started:" << m_cooldownSeconds << "seconds";
}

void MainWindow::onCooldownTick() {
    m_remainingCooldown--;

    if (m_remainingCooldown <= 0) {
        // 冷却结束
        m_cooldownTimer->stop();
        m_isCooldownActive = false;
        m_cooldownLabel->setVisible(false);
        m_postListPanel->setEnabled(true);
        m_statusLabel->setText("状态: 冷却结束，可以继续关注");
        qDebug() << "[INFO] Cooldown ended";
    } else {
        // 更新倒计时显示
        updateCooldownDisplay();
    }
}

void MainWindow::updateCooldownDisplay() {
    QString text = QString("冷却中: %1 秒后可继续关注").arg(m_remainingCooldown);
    m_cooldownLabel->setText(text);
    m_statusLabel->setText(QString("状态: 冷却中，请等待 %1 秒").arg(m_remainingCooldown));
}

void MainWindow::updateFollowedAuthorsTable() {
    m_followedAuthorsTable->setRowCount(0);

    for (const auto& post : m_posts) {
        if (!post.isFollowed) {
            continue;
        }

        int row = m_followedAuthorsTable->rowCount();
        m_followedAuthorsTable->insertRow(row);

        // 作者
        QTableWidgetItem* authorItem = new QTableWidgetItem("@" + post.authorHandle);
        authorItem->setData(Qt::UserRole, post.authorHandle);
        authorItem->setToolTip(post.authorName);
        m_followedAuthorsTable->setItem(row, 0, authorItem);

        // 关注时间
        QString timeStr = post.followTime.isValid()
            ? post.followTime.toString("MM-dd HH:mm")
            : "-";
        m_followedAuthorsTable->setItem(row, 1, new QTableWidgetItem(timeStr));

        // 来源帖子
        QString contentPreview = post.content.left(30);
        if (post.content.length() > 30) {
            contentPreview += "...";
        }
        QTableWidgetItem* contentItem = new QTableWidgetItem(contentPreview);
        contentItem->setToolTip(post.content);
        m_followedAuthorsTable->setItem(row, 2, contentItem);
    }

    // 更新Tab标题显示数量
    int followedCount = m_followedAuthorsTable->rowCount();
    m_tabWidget->setTabText(1, QString("已关注(%1)").arg(followedCount));
}

void MainWindow::onFollowedAuthorDoubleClicked(int row, int column) {
    Q_UNUSED(column);

    if (row < 0 || row >= m_followedAuthorsTable->rowCount()) {
        return;
    }

    // 获取作者handle
    QTableWidgetItem* item = m_followedAuthorsTable->item(row, 0);
    if (!item) {
        return;
    }

    QString authorHandle = item->data(Qt::UserRole).toString();
    if (authorHandle.isEmpty()) {
        return;
    }

    m_statusLabel->setText(QString("状态: 正在打开 @%1 的主页(仅查看)...").arg(authorHandle));

    // 首次点击时初始化右侧浏览器
    if (!m_userBrowserInitialized && m_userBrowser) {
        m_userBrowserInitialized = true;
        QString profilePath = m_dataStorage->getProfilePath();
        QString userUrl = QString("https://x.com/%1").arg(authorHandle);
        qDebug() << "[INFO] Creating user browser for viewing:" << authorHandle;
        m_userBrowser->CreateBrowserWithProfile(userUrl, profilePath);
    } else {
        // 浏览器已初始化，直接加载URL（不触发自动关注）
        QString userUrl = QString("https://x.com/%1").arg(authorHandle);
        m_userBrowser->LoadUrl(userUrl);
    }

    // 注意：不设置 m_currentFollowingHandle，所以不会触发自动关注
    qDebug() << "[INFO] Viewing followed author:" << authorHandle;
}
