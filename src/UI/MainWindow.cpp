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
#include <QJsonObject>
#include <QStatusBar>
#include <QSet>
#include <QHeaderView>
#include <QUrl>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QDesktopServices>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mainSplitter(nullptr)
    , m_leftSplitter(nullptr)
    , m_searchBrowser(nullptr)
    , m_followersBrowser(nullptr)
    , m_followersBrowserInitialized(false)
    , m_centerPanel(nullptr)
    , m_keywordPanel(nullptr)
    , m_postListPanel(nullptr)
    , m_hideFollowedCheckBox(nullptr)
    , m_cooldownMinSpinBox(nullptr)
    , m_cooldownMaxSpinBox(nullptr)
    , m_autoFollowBtn(nullptr)
    , m_unfollowDaysSpinBox(nullptr)
    , m_rightPanel(nullptr)
    , m_cooldownLabel(nullptr)
    , m_userBrowser(nullptr)
    , m_logTextEdit(nullptr)
    , m_statusLabel(nullptr)
    , m_dataStorage(nullptr)
    , m_postMonitor(nullptr)
    , m_autoFollower(nullptr)
    , m_cefTimerId(0)
    , m_searchBrowserInitialized(false)
    , m_userBrowserInitialized(false)
    , m_cooldownTimer(nullptr)
    , m_cooldownMinSeconds(60)
    , m_cooldownMaxSeconds(180)
    , m_remainingCooldown(0)
    , m_isCooldownActive(false)
    , m_isAutoFollowing(false)
    , m_autoRefreshTimer(nullptr)
    , m_currentKeywordIndex(0)
    , m_followersSwitchTimer(nullptr)
    , m_currentFollowedUserIndex(-1)
    , m_isCheckingFollowBack(false)
    , m_consecutiveFailures(0)
    , m_isSleeping(false)
    , m_remainingSleepSeconds(0)
    , m_sleepTimer(nullptr) {

    setWindowTitle("X互关宝 - X.com互关粉丝助手");
    resize(1600, 900);

    // 初始化冷却计时器
    m_cooldownTimer = new QTimer(this);
    connect(m_cooldownTimer, &QTimer::timeout, this, &MainWindow::onCooldownTick);

    // 初始化自动刷新计时器
    m_autoRefreshTimer = new QTimer(this);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &MainWindow::onAutoRefreshTimeout);

    // 初始化休眠计时器
    m_sleepTimer = new QTimer(this);
    connect(m_sleepTimer, &QTimer::timeout, this, &MainWindow::onSleepTick);

    // 初始化粉丝切换定时器
    m_followersSwitchTimer = new QTimer(this);
    connect(m_followersSwitchTimer, &QTimer::timeout, this, &MainWindow::onFollowersSwitchTimeout);

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

    // 左侧 - 垂直分割器（搜索浏览器 + 粉丝浏览器）
    m_leftSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
    m_leftSplitter->setMinimumWidth(500);

    // 搜索浏览器（上半部分）
    m_searchBrowser = new BrowserWidget(m_leftSplitter);
    m_searchBrowser->setMinimumHeight(300);

    // 粉丝浏览器（下半部分）
    m_followersBrowser = new BrowserWidget(m_leftSplitter);
    m_followersBrowser->setMinimumHeight(300);

    // 设置1:1比例
    m_leftSplitter->setSizes({450, 450});

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

    // 冷却时间设置（随机范围）
    QHBoxLayout* cooldownLayout = new QHBoxLayout();
    QLabel* cooldownSettingLabel = new QLabel("关注冷却(秒):", m_centerPanel);
    m_cooldownMinSpinBox = new QSpinBox(m_centerPanel);
    m_cooldownMinSpinBox->setRange(60, 300);
    m_cooldownMinSpinBox->setValue(m_cooldownMinSeconds);
    QLabel* toLabel = new QLabel("~", m_centerPanel);
    m_cooldownMaxSpinBox = new QSpinBox(m_centerPanel);
    m_cooldownMaxSpinBox->setRange(60, 600);
    m_cooldownMaxSpinBox->setValue(m_cooldownMaxSeconds);

    // 最小值变化时，确保最大值 >= 最小值
    connect(m_cooldownMinSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_cooldownMaxSpinBox->value() < value) {
            m_cooldownMaxSpinBox->setValue(value);
        }
        m_cooldownMaxSpinBox->setMinimum(value);
    });

    // 最大值变化时，确保最大值 >= 最小值
    connect(m_cooldownMaxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (value < m_cooldownMinSpinBox->value()) {
            m_cooldownMaxSpinBox->setValue(m_cooldownMinSpinBox->value());
        }
    });

    cooldownLayout->addWidget(cooldownSettingLabel);
    cooldownLayout->addWidget(m_cooldownMinSpinBox);
    cooldownLayout->addWidget(toLabel);
    cooldownLayout->addWidget(m_cooldownMaxSpinBox);

    // 自动关注按钮
    m_autoFollowBtn = new QPushButton("自动关注", m_centerPanel);
    m_autoFollowBtn->setCheckable(true);
    m_autoFollowBtn->setToolTip("自动从上往下批量关注未关注的用户");
    m_autoFollowBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #1da1f2;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 5px 15px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1a91da;"
        "}"
        "QPushButton:checked {"
        "  background-color: #e0245e;"
        "}"
        "QPushButton:checked:hover {"
        "  background-color: #c81e54;"
        "}"
    );
    cooldownLayout->addWidget(m_autoFollowBtn);

    // 取关天数设置
    QLabel* unfollowDaysLabel = new QLabel("未回关取关天数:", m_centerPanel);
    unfollowDaysLabel->setToolTip("关注超过此天数且未回关的用户将被取消关注");
    m_unfollowDaysSpinBox = new QSpinBox(m_centerPanel);
    m_unfollowDaysSpinBox->setRange(1, 30);
    m_unfollowDaysSpinBox->setValue(2);
    m_unfollowDaysSpinBox->setToolTip("关注超过此天数且未回关的用户将被取消关注");
    cooldownLayout->addWidget(unfollowDaysLabel);
    cooldownLayout->addWidget(m_unfollowDaysSpinBox);

    cooldownLayout->addStretch();

    // 打开数据文件夹按钮
    QPushButton* openDataFolderBtn = new QPushButton("Data", m_centerPanel);
    openDataFolderBtn->setToolTip("Open data folder");
    openDataFolderBtn->setFixedWidth(50);
    openDataFolderBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #6c757d;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 3px;"
        "  padding: 3px 8px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5a6268;"
        "}"
    );
    connect(openDataFolderBtn, &QPushButton::clicked, this, [this]() {
        QString path = m_dataStorage->getDataPath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    cooldownLayout->addWidget(openDataFolderBtn);

    // 作者链接
    QLabel* xLink = new QLabel("<a href=\"https://x.com/4111y80y\">X</a>", m_centerPanel);
    xLink->setOpenExternalLinks(true);
    xLink->setToolTip("https://x.com/4111y80y");
    QLabel* githubLink = new QLabel("<a href=\"https://github.com/4111y80y/xfollowing\">GitHub</a>", m_centerPanel);
    githubLink->setOpenExternalLinks(true);
    githubLink->setToolTip("https://github.com/4111y80y/xfollowing");
    cooldownLayout->addWidget(xLink);
    cooldownLayout->addWidget(new QLabel("|", m_centerPanel));
    cooldownLayout->addWidget(githubLink);

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

    // 操作提示标签（浏览器未初始化时显示）
    m_hintLabel = new QLabel(m_rightPanel);
    m_hintLabel->setText("点击左侧帖子列表中的帖子\n或点击\"自动关注\"按钮\n开始关注用户");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_hintLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #f0f0f0;"
        "  color: #666666;"
        "  font-size: 18px;"
        "  padding: 40px;"
        "}"
    );
    rightLayout->addWidget(m_hintLabel, 1);

    // 用户浏览器（初始隐藏）
    m_userBrowser = new BrowserWidget(m_rightPanel);
    m_userBrowser->setMinimumWidth(500);
    m_userBrowser->setVisible(false);
    rightLayout->addWidget(m_userBrowser, 1);

    // 日志信息框（底部，终端风格）
    m_logTextEdit = new QTextEdit(m_rightPanel);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_logTextEdit->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #00ff00;"
        "  font-family: Consolas, monospace;"
        "  font-size: 11px;"
        "  border: 1px solid #333;"
        "}"
    );
    rightLayout->addWidget(m_logTextEdit, 1);

    // 设置分栏比例
    m_mainSplitter->setSizes({500, 350, 500});

    // 状态栏
    m_statusLabel = new QLabel("状态: 就绪");
    m_statsLabel = new QLabel("已采集: 0 | 已关注: 0 | 待关注: 0");
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_statsLabel);

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
    connect(m_userBrowser, &BrowserWidget::accountSuspended, this, &MainWindow::onAccountSuspended);
    // 回关检查信号
    connect(m_userBrowser, &BrowserWidget::checkFollowsBack, this, &MainWindow::onCheckFollowsBack);
    connect(m_userBrowser, &BrowserWidget::checkNotFollowBack, this, &MainWindow::onCheckNotFollowBack);
    connect(m_userBrowser, &BrowserWidget::checkSuspended, this, &MainWindow::onCheckSuspended);
    connect(m_userBrowser, &BrowserWidget::checkNotFollowing, this, &MainWindow::onCheckNotFollowing);
    connect(m_userBrowser, &BrowserWidget::unfollowSuccess, this, &MainWindow::onUnfollowSuccess);
    connect(m_userBrowser, &BrowserWidget::unfollowFailed, this, &MainWindow::onUnfollowFailed);

    // 帖子列表点击
    connect(m_postListPanel, &PostListPanel::postClicked, this, &MainWindow::onPostClicked);

    // 隐藏已关注开关
    connect(m_hideFollowedCheckBox, &QCheckBox::toggled, this, &MainWindow::onHideFollowedChanged);

    // 关键词变化
    connect(m_keywordPanel, &KeywordPanel::keywordsChanged, this, &MainWindow::onKeywordsChanged);

    // 双击关键词跳转到Latest搜索
    connect(m_keywordPanel, &KeywordPanel::keywordDoubleClicked, this, &MainWindow::onKeywordDoubleClicked);

    // 自动关注按钮
    connect(m_autoFollowBtn, &QPushButton::toggled, this, &MainWindow::onAutoFollowToggled);

    // 已关注作者单击
    connect(m_followedAuthorsTable, &QTableWidget::cellClicked, this, &MainWindow::onFollowedAuthorDoubleClicked);

    // 粉丝浏览器信号
    connect(m_followersBrowser, &BrowserWidget::browserCreated, this, &MainWindow::onFollowersBrowserCreated);
    connect(m_followersBrowser, &BrowserWidget::loadFinished, this, &MainWindow::onFollowersLoadFinished);
    connect(m_followersBrowser, &BrowserWidget::newFollowersFound, this, &MainWindow::onNewFollowersFound);
}

void MainWindow::loadSettings() {
    QSettings settings("xfollowing", "X互关宝");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    if (settings.contains("splitterSizes")) {
        m_mainSplitter->restoreState(settings.value("splitterSizes").toByteArray());
    }

    // 隐藏已关注（默认true）
    m_hideFollowedCheckBox->setChecked(settings.value("hideFollowed", true).toBool());

    // 冷却时间设置
    m_cooldownMinSeconds = settings.value("cooldownMin", 60).toInt();
    m_cooldownMaxSeconds = settings.value("cooldownMax", 180).toInt();
    m_cooldownMinSpinBox->setValue(m_cooldownMinSeconds);
    m_cooldownMaxSpinBox->setValue(m_cooldownMaxSeconds);

    // 取关天数设置
    m_unfollowDaysSpinBox->setValue(settings.value("unfollowDays", 2).toInt());
}

void MainWindow::saveSettings() {
    QSettings settings("xfollowing", "X互关宝");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("splitterSizes", m_mainSplitter->saveState());
    settings.setValue("hideFollowed", m_hideFollowedCheckBox->isChecked());

    // 保存冷却时间设置
    settings.setValue("cooldownMin", m_cooldownMinSpinBox->value());
    settings.setValue("cooldownMax", m_cooldownMaxSpinBox->value());

    // 保存取关天数设置
    settings.setValue("unfollowDays", m_unfollowDaysSpinBox->value());
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
    if (m_followersBrowser) {
        m_followersBrowser->CloseBrowser();
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
        m_searchBrowser->CreateBrowserWithProfile("https://x.com/search?q=%E4%BA%92%E5%85%B3%20filter%3Ablue_verified&f=live", profilePath);
    }

    // 粉丝浏览器延迟创建，在搜索页面加载完成后创建，避免同时初始化同一配置目录
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

        // 启动自动刷新定时器（60-180秒随机）
        int refreshInterval = m_cooldownMinSeconds + (rand() % (m_cooldownMaxSeconds - m_cooldownMinSeconds + 1));
        m_autoRefreshTimer->start(refreshInterval * 1000);
        qDebug() << "[INFO] Auto-refresh timer started, next refresh in" << refreshInterval << "seconds";

        // 延迟创建粉丝浏览器（等搜索浏览器完全初始化后）
        if (!m_followersBrowserInitialized && m_followersBrowser) {
            QTimer::singleShot(3000, this, [this]() {
                if (!m_followersBrowserInitialized && m_followersBrowser) {
                    m_followersBrowserInitialized = true;
                    QString profilePath = m_dataStorage->getProfilePath();
                    qDebug() << "[INFO] Creating followers browser with profile:" << profilePath;
                    m_followersBrowser->CreateBrowserWithProfile("https://x.com", profilePath);
                }
            });
        }
    } else {
        m_statusLabel->setText("状态: 搜索页面加载失败");
    }
}

void MainWindow::onUserLoadFinished(bool success) {
    if (!success) {
        return;
    }

    // 回关检查模式
    if (m_isCheckingFollowBack && !m_currentCheckingHandle.isEmpty()) {
        qDebug() << "[INFO] User page loaded, executing check follow-back script for:" << m_currentCheckingHandle;
        QString script = m_autoFollower->getCheckFollowBackScript();
        m_userBrowser->ExecuteJavaScript(script);
        return;
    }

    // 关注模式
    if (!m_currentFollowingHandle.isEmpty()) {
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
        m_hintLabel->setVisible(false);
        m_userBrowser->setVisible(true);
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

    // 固定作者的handle
    const QString pinnedAuthorHandle = "4111y80y";

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

        // 解析帖子发布时间
        QString postTimeStr = obj["postTime"].toString();
        if (!postTimeStr.isEmpty()) {
            post.postTime = QDateTime::fromString(postTimeStr, Qt::ISODate);
        } else {
            post.postTime = QDateTime::currentDateTime();
        }

        // 跳过无效数据
        if (post.authorHandle.isEmpty() || post.postId.isEmpty()) {
            continue;
        }

        // 去重：按作者去重（同一作者只保留一条帖子，因为目的是关注用户）
        bool exists = false;
        for (int i = 0; i < m_posts.size(); ++i) {
            // 按postId去重，或者按作者去重
            if (m_posts[i].postId == post.postId || m_posts[i].authorHandle == post.authorHandle) {
                exists = true;
                // 如果是未关注的用户再次出现，更新采集时间使其前置（激活）
                if (!m_posts[i].isFollowed) {
                    m_posts[i].collectTime = QDateTime::currentDateTime();
                    m_dataStorage->updatePost(m_posts[i]);
                    newCount++;  // 标记有变化，需要重新排序
                }
                break;
            }
        }

        if (!exists) {
            m_posts.append(post);
            m_dataStorage->addPost(post);
            newCount++;
        }
    }

    if (newCount > 0) {
        // 排序：固定帖子始终第一，其他按采集时间降序（最新发现的在前）
        std::sort(m_posts.begin(), m_posts.end(), [&pinnedAuthorHandle](const Post& a, const Post& b) {
            // 固定帖子始终排第一
            if (a.authorHandle == pinnedAuthorHandle) return true;
            if (b.authorHandle == pinnedAuthorHandle) return false;
            // 其他按采集时间降序（最新发现的在前）
            return a.collectTime > b.collectTime;
        });

        m_postListPanel->setPosts(m_posts);
        updateStatusBar();
        qDebug() << "[INFO] Found" << newCount << "new posts";
    }
}

void MainWindow::onFollowSuccess(const QString& userHandle) {
    qDebug() << "[SUCCESS] Followed:" << userHandle;

    // 记录日志
    appendLog(QString("关注 @%1 成功").arg(m_currentFollowingHandle));

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

    // 记录日志
    appendLog(QString("@%1 已关注，跳过").arg(m_currentFollowingHandle));

    // 更新帖子状态
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts[i].authorHandle == m_currentFollowingHandle) {
            m_posts[i].isFollowed = true;
            m_dataStorage->updatePost(m_posts[i]);
        }
    }

    m_postListPanel->setPosts(m_posts);
    updateFollowedAuthorsTable();
    updateStatusBar();

    m_statusLabel->setText(QString("状态: @%1 已经关注过了").arg(m_currentFollowingHandle));
    m_currentFollowingHandle.clear();

    // 如果是自动关注模式，跳过此用户，继续处理下一个（无需冷却）
    if (m_isAutoFollowing) {
        QTimer::singleShot(1000, this, &MainWindow::processNextAutoFollow);
    }
}

void MainWindow::onFollowFailed(const QString& userHandle) {
    qDebug() << "[ERROR] Follow failed:" << userHandle;

    // 增加连续失败计数
    m_consecutiveFailures++;

    // 记录日志
    appendLog(QString("关注 @%1 失败 (连续%2次)").arg(m_currentFollowingHandle).arg(m_consecutiveFailures));
    m_statusLabel->setText(QString("状态: 关注 @%1 失败").arg(m_currentFollowingHandle));
    m_currentFollowingHandle.clear();

    // 连续失败3次，进入30分钟休眠
    if (m_consecutiveFailures >= 3 && m_isAutoFollowing) {
        appendLog("连续失败3次，进入30分钟休眠...");
        startSleep();
        return;
    }

    // 如果是自动关注模式，跳过此用户，继续处理下一个（无需冷却）
    if (m_isAutoFollowing) {
        QTimer::singleShot(1000, this, &MainWindow::processNextAutoFollow);
    }
}

void MainWindow::onAccountSuspended(const QString& userHandle) {
    qDebug() << "[WARNING] Account suspended:" << userHandle;

    // 记录日志
    appendLog(QString("@%1 账号被封禁，已删除").arg(userHandle));
    m_statusLabel->setText(QString("状态: @%1 账号已被封禁，已删除").arg(userHandle));

    // 从帖子列表中删除该用户的所有帖子
    for (int i = m_posts.size() - 1; i >= 0; --i) {
        if (m_posts[i].authorHandle == userHandle) {
            m_posts.removeAt(i);
        }
    }

    // 保存并更新界面
    m_dataStorage->savePosts(m_posts);
    m_postListPanel->setPosts(m_posts);
    updateStatusBar();

    m_currentFollowingHandle.clear();

    // 如果是自动关注模式，继续处理下一个（无需冷却）
    if (m_isAutoFollowing) {
        QTimer::singleShot(1000, this, &MainWindow::processNextAutoFollow);
    }
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
    m_statsLabel->setText(status);
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
    const QString pinnedContent = "X互关宝作者 - 欢迎互关交流!";

    // 检查是否已存在，如果存在则更新内容
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts[i].postId == pinnedPostId || m_posts[i].authorHandle == pinnedAuthorHandle) {
            // 更新内容（确保没有[固定]字样）
            if (m_posts[i].content != pinnedContent) {
                m_posts[i].content = pinnedContent;
                m_dataStorage->updatePost(m_posts[i]);
            }
            return;
        }
    }

    // 创建固定帖子
    Post pinnedPost;
    pinnedPost.postId = pinnedPostId;
    pinnedPost.authorHandle = pinnedAuthorHandle;
    pinnedPost.authorName = "X互关宝作者";
    pinnedPost.authorUrl = "https://x.com/" + pinnedAuthorHandle;
    pinnedPost.content = pinnedContent;
    pinnedPost.postUrl = "https://x.com/" + pinnedAuthorHandle + "/status/" + pinnedPostId;
    pinnedPost.matchedKeyword = "互关";
    pinnedPost.collectTime = QDateTime::currentDateTime();
    pinnedPost.isFollowed = false;

    // 添加到列表开头
    m_posts.prepend(pinnedPost);
}

void MainWindow::startCooldown() {
    // 获取用户设置的冷却时间范围
    m_cooldownMinSeconds = m_cooldownMinSpinBox->value();
    m_cooldownMaxSeconds = m_cooldownMaxSpinBox->value();

    // 确保最大值不小于最小值
    if (m_cooldownMaxSeconds < m_cooldownMinSeconds) {
        m_cooldownMaxSeconds = m_cooldownMinSeconds;
    }

    // 生成随机冷却时间
    int randomCooldown = m_cooldownMinSeconds + (rand() % (m_cooldownMaxSeconds - m_cooldownMinSeconds + 1));
    m_remainingCooldown = randomCooldown;
    m_isCooldownActive = true;

    // 禁用帖子列表点击
    m_postListPanel->setEnabled(false);

    // 显示倒计时
    updateCooldownDisplay();
    m_cooldownLabel->setVisible(true);

    // 启动计时器（每秒触发一次）
    m_cooldownTimer->start(1000);

    // 在冷却期间开始回关检查
    QTimer::singleShot(3000, this, &MainWindow::startFollowBackCheck);

    qDebug() << "[INFO] Cooldown started:" << randomCooldown << "seconds (range:" << m_cooldownMinSeconds << "-" << m_cooldownMaxSeconds << ")";
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

        // 如果自动关注开启，继续处理下一个
        if (m_isAutoFollowing) {
            processNextAutoFollow();
        }
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
        m_hintLabel->setVisible(false);
        m_userBrowser->setVisible(true);
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

void MainWindow::onKeywordDoubleClicked(const QString& keyword) {
    qDebug() << "[INFO] Keyword double-clicked:" << keyword;

    // 构建Latest搜索URL (f=live表示Latest/最新)
    QString encodedKeyword = QUrl::toPercentEncoding(keyword);
    QString searchUrl = QString("https://x.com/search?q=%1%20filter%3Ablue_verified&f=live").arg(encodedKeyword);

    m_statusLabel->setText(QString("状态: 正在搜索关键词 \"%1\" 的最新帖子...").arg(keyword));

    // 左侧浏览器加载搜索页面
    if (m_searchBrowserInitialized && m_searchBrowser) {
        m_searchBrowser->LoadUrl(searchUrl);
        qDebug() << "[INFO] Loading search URL:" << searchUrl;
    }
}

void MainWindow::onAutoFollowToggled() {
    // 如果正在冷却中，不允许启动自动关注
    if (m_isCooldownActive && m_autoFollowBtn->isChecked()) {
        m_autoFollowBtn->setChecked(false);
        m_statusLabel->setText("状态: 冷却中，请等待冷却结束后再启动自动关注");
        return;
    }

    m_isAutoFollowing = m_autoFollowBtn->isChecked();

    if (m_isAutoFollowing) {
        m_autoFollowBtn->setText("停止关注");
        m_statusLabel->setText("状态: 自动关注已启动");
        qDebug() << "[INFO] Auto-follow started";

        // 禁用帖子列表和已关注列表，防止手动操作干扰
        m_postListPanel->setEnabled(false);
        m_followedAuthorsTable->setEnabled(false);

        // 如果当前没有在冷却中，立即开始处理
        if (!m_isCooldownActive) {
            processNextAutoFollow();
        }
    } else {
        m_autoFollowBtn->setText("自动关注");
        m_statusLabel->setText("状态: 自动关注已停止");
        qDebug() << "[INFO] Auto-follow stopped";

        // 恢复帖子列表和已关注列表的点击
        m_postListPanel->setEnabled(true);
        m_followedAuthorsTable->setEnabled(true);
    }
}

void MainWindow::processNextAutoFollow() {
    if (!m_isAutoFollowing) {
        return;
    }

    // 固定作者的handle（优先处理）
    const QString pinnedAuthorHandle = "4111y80y";

    // 优先查找固定帖子（如果未关注）
    for (const auto& post : m_posts) {
        if (post.authorHandle == pinnedAuthorHandle && !post.isFollowed) {
            // 固定帖子未关注，优先处理
            qDebug() << "[INFO] Auto-follow: processing pinned author" << post.authorHandle;
            m_currentFollowingHandle = post.authorHandle;
            m_statusLabel->setText(QString("状态: [自动] 正在关注 @%1...").arg(post.authorHandle));

            if (!m_userBrowserInitialized && m_userBrowser) {
                m_userBrowserInitialized = true;
                m_hintLabel->setVisible(false);
                m_userBrowser->setVisible(true);
                QString profilePath = m_dataStorage->getProfilePath();
                QString userUrl = QString("https://x.com/%1").arg(post.authorHandle);
                m_userBrowser->CreateBrowserWithProfile(userUrl, profilePath);
            } else {
                QString userUrl = QString("https://x.com/%1").arg(post.authorHandle);
                m_userBrowser->LoadUrl(userUrl);
            }
            return;
        }
    }

    // 然后查找其他未关注的帖子
    for (const auto& post : m_posts) {
        // 跳过固定帖子（已在上面处理）
        if (post.authorHandle == pinnedAuthorHandle) {
            continue;
        }
        // 跳过已关注的
        if (post.isFollowed) {
            continue;
        }

        // 找到了，执行关注
        qDebug() << "[INFO] Auto-follow: processing" << post.authorHandle;
        m_currentFollowingHandle = post.authorHandle;
        m_statusLabel->setText(QString("状态: [自动] 正在关注 @%1...").arg(post.authorHandle));

        // 首次点击时初始化右侧浏览器
        if (!m_userBrowserInitialized && m_userBrowser) {
            m_userBrowserInitialized = true;
            m_hintLabel->setVisible(false);
            m_userBrowser->setVisible(true);
            QString profilePath = m_dataStorage->getProfilePath();
            QString userUrl = QString("https://x.com/%1").arg(post.authorHandle);
            m_userBrowser->CreateBrowserWithProfile(userUrl, profilePath);
        } else {
            QString userUrl = QString("https://x.com/%1").arg(post.authorHandle);
            m_userBrowser->LoadUrl(userUrl);
        }
        return;
    }

    // 没有找到未关注的帖子
    m_isAutoFollowing = false;
    m_autoFollowBtn->setChecked(false);
    m_autoFollowBtn->setText("自动关注");
    m_statusLabel->setText("状态: 自动关注完成，没有更多待关注用户");

    // 恢复列表点击
    m_postListPanel->setEnabled(true);
    m_followedAuthorsTable->setEnabled(true);

    qDebug() << "[INFO] Auto-follow completed: no more users to follow";
}

void MainWindow::onAutoRefreshTimeout() {
    // 获取所有启用的关键词
    QList<Keyword> enabledKeywords;
    for (const auto& kw : m_keywords) {
        if (kw.isEnabled) {
            enabledKeywords.append(kw);
        }
    }

    if (enabledKeywords.isEmpty()) {
        qDebug() << "[INFO] No enabled keywords, stopping auto-refresh";
        m_autoRefreshTimer->stop();
        return;
    }

    // 切换到下一个关键词
    m_currentKeywordIndex = (m_currentKeywordIndex + 1) % enabledKeywords.size();
    const QString& keyword = enabledKeywords[m_currentKeywordIndex].text;

    qDebug() << "[INFO] Auto-switch to keyword:" << keyword;

    // 记录日志
    appendLog(QString("切换搜索关键词: %1").arg(keyword));
    m_statusLabel->setText(QString("状态: 切换到关键词 \"%1\"...").arg(keyword));

    // 构建Latest搜索URL (f=live表示Latest/最新)
    QString encodedKeyword = QUrl::toPercentEncoding(keyword);
    QString searchUrl = QString("https://x.com/search?q=%1%20filter%3Ablue_verified&f=live").arg(encodedKeyword);

    // 左侧浏览器加载新的搜索页面
    if (m_searchBrowser) {
        m_searchBrowser->LoadUrl(searchUrl);
    }

    // 设置下一次切换时间（30-60秒随机）
    int switchInterval = 30 + (rand() % 31);  // 30-60秒随机
    m_autoRefreshTimer->start(switchInterval * 1000);
    qDebug() << "[INFO] Next keyword switch in" << switchInterval << "seconds";
}

void MainWindow::startFollowBackCheck() {
    if (m_isCheckingFollowBack) {
        return;  // 已经在检查中
    }
    m_isCheckingFollowBack = true;
    checkNextFollowBack();
}

void MainWindow::checkNextFollowBack() {
    if (!m_isCooldownActive) {
        // 冷却结束，停止检查
        m_isCheckingFollowBack = false;
        m_currentCheckingHandle.clear();
        return;
    }

    // 获取取关天数设置
    int unfollowDays = m_unfollowDaysSpinBox->value();
    QDateTime now = QDateTime::currentDateTime();
    QDateTime unfollowThreshold = now.addDays(-unfollowDays);
    QDateTime oneWeekAgo = now.addDays(-7);

    // 按关注时间排序，从最早关注的开始检查
    Post* oldestUnchecked = nullptr;
    for (int i = 0; i < m_posts.size(); ++i) {
        Post& post = m_posts[i];
        // 必须是已关注的
        if (!post.isFollowed) {
            continue;
        }
        // 跳过固定作者
        if (post.authorHandle == "4111y80y") {
            continue;
        }
        // 必须关注超过指定天数
        if (!post.followTime.isValid() || post.followTime > unfollowThreshold) {
            continue;  // 关注时间不足，跳过
        }
        // 检查是否超过7天未检查
        if (post.lastCheckedTime.isValid() && post.lastCheckedTime > oneWeekAgo) {
            continue;  // 7天内检查过，跳过
        }
        // 找到最早关注的未检查用户
        if (!oldestUnchecked || post.followTime < oldestUnchecked->followTime) {
            oldestUnchecked = &m_posts[i];
        }
    }

    if (!oldestUnchecked) {
        // 没有需要检查的用户
        m_isCheckingFollowBack = false;
        m_currentCheckingHandle.clear();
        qDebug() << "[INFO] No users need follow-back check";
        return;
    }

    // 开始检查这个用户
    m_currentCheckingHandle = oldestUnchecked->authorHandle;
    qDebug() << "[INFO] Checking follow-back for:" << m_currentCheckingHandle;

    // 记录日志
    appendLog(QString("开始检查 @%1 是否回关").arg(m_currentCheckingHandle));

    // 醒目显示正在检查
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #f0ad4e; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setText(QString("正在检查 @%1 是否回关...").arg(m_currentCheckingHandle));
    m_statusLabel->setText(QString("状态: 冷却中，检查 @%1 是否回关...").arg(m_currentCheckingHandle));

    // 打开用户主页
    QString userUrl = QString("https://x.com/%1").arg(m_currentCheckingHandle);
    m_userBrowser->LoadUrl(userUrl);

    // 页面加载后会触发 onUserLoadFinished，在那里注入检查脚本
}

void MainWindow::onCheckFollowsBack(const QString& userHandle) {
    qDebug() << "[INFO] User follows back:" << userHandle;

    // 记录日志
    appendLog(QString("@%1 已回关").arg(userHandle));

    // 更新检查时间
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts[i].authorHandle == userHandle) {
            m_posts[i].lastCheckedTime = QDateTime::currentDateTime();
        }
    }
    m_dataStorage->savePosts(m_posts);

    // 醒目显示：已回关（绿色）
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #5cb85c; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setText(QString("@%1 已回关! 冷却中: %2 秒").arg(userHandle).arg(m_remainingCooldown));
    m_statusLabel->setText(QString("状态: @%1 已回关").arg(userHandle));

    m_currentCheckingHandle.clear();
    m_isCheckingFollowBack = false;
    // 不再继续检查，等待下一次冷却
}

void MainWindow::onCheckNotFollowBack(const QString& userHandle) {
    qDebug() << "[WARNING] User does NOT follow back:" << userHandle;

    // 计算关注了多少天
    int followedDays = 0;
    for (const auto& post : m_posts) {
        if (post.authorHandle == userHandle && post.followTime.isValid()) {
            followedDays = post.followTime.daysTo(QDateTime::currentDateTime());
            break;
        }
    }

    // 记录日志（显示关注了多久）
    appendLog(QString("@%1 关注%2天未回关，取消关注").arg(userHandle).arg(followedDays));

    // 醒目显示：没有回关，正在取消（红色）
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #d9534f; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setText(QString("@%1 没有回关! 正在取消关注...").arg(userHandle));
    m_statusLabel->setText(QString("状态: @%1 没有回关，正在取消关注...").arg(userHandle));

    // 执行取消关注脚本（取消关注完成后会停止检查）
    QString script = m_autoFollower->getUnfollowScript();
    m_userBrowser->ExecuteJavaScript(script);
}

void MainWindow::onCheckSuspended(const QString& userHandle) {
    qDebug() << "[WARNING] Account suspended during check:" << userHandle;

    // 记录日志
    appendLog(QString("@%1 账号被封禁").arg(userHandle));

    // 醒目显示：账号被封禁（深红色）
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #c9302c; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setText(QString("@%1 账号已被封禁! 已删除").arg(userHandle));
    m_statusLabel->setText(QString("状态: @%1 账号已被封禁，已删除").arg(userHandle));

    // 删除该用户的所有帖子
    for (int i = m_posts.size() - 1; i >= 0; --i) {
        if (m_posts[i].authorHandle == userHandle) {
            m_posts.removeAt(i);
        }
    }
    m_dataStorage->savePosts(m_posts);
    m_postListPanel->setPosts(m_posts);
    updateStatusBar();
    updateFollowedAuthorsTable();

    m_currentCheckingHandle.clear();
    m_isCheckingFollowBack = false;
    // 不再继续检查，等待下一次冷却
}

void MainWindow::onCheckNotFollowing(const QString& userHandle) {
    qDebug() << "[INFO] Not following user:" << userHandle;

    // 更新记录，标记为未关注
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts[i].authorHandle == userHandle) {
            m_posts[i].isFollowed = false;
            m_posts[i].lastCheckedTime = QDateTime::currentDateTime();
        }
    }
    m_dataStorage->savePosts(m_posts);
    m_postListPanel->setPosts(m_posts);
    updateStatusBar();
    updateFollowedAuthorsTable();

    // 醒目显示：记录已更新（蓝色）
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #5bc0de; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setText(QString("@%1 记录已更新，冷却中: %2 秒").arg(userHandle).arg(m_remainingCooldown));
    m_statusLabel->setText(QString("状态: @%1 记录已更新").arg(userHandle));

    m_currentCheckingHandle.clear();
    m_isCheckingFollowBack = false;
    // 不再继续检查，等待下一次冷却
}

void MainWindow::onUnfollowSuccess(const QString& userHandle) {
    qDebug() << "[INFO] Unfollow success:" << userHandle;

    // 记录日志
    appendLog(QString("已取消关注 @%1").arg(userHandle));

    // 醒目显示：已取消关注（橙色）
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #f0ad4e; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setText(QString("已取消关注 @%1，冷却中: %2 秒").arg(userHandle).arg(m_remainingCooldown));
    m_statusLabel->setText(QString("状态: 已取消关注 @%1").arg(userHandle));

    // 删除该用户的所有帖子记录（从去重中释放，后续可以重新关注）
    for (int i = m_posts.size() - 1; i >= 0; --i) {
        if (m_posts[i].authorHandle == userHandle) {
            m_posts.removeAt(i);
        }
    }
    m_dataStorage->savePosts(m_posts);
    m_postListPanel->setPosts(m_posts);
    updateStatusBar();
    updateFollowedAuthorsTable();

    m_currentCheckingHandle.clear();
    m_isCheckingFollowBack = false;
    // 不再继续检查，等待下一次冷却
}

void MainWindow::onUnfollowFailed(const QString& userHandle) {
    qDebug() << "[ERROR] Unfollow failed:" << userHandle;

    // 记录日志
    appendLog(QString("取消关注 @%1 失败").arg(userHandle));

    // 醒目显示：取消关注失败（红色）
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #d9534f; color: white; font-size: 16px; font-weight: bold; padding: 10px; }");
    m_cooldownLabel->setText(QString("取消关注 @%1 失败，冷却中: %2 秒").arg(userHandle).arg(m_remainingCooldown));
    m_statusLabel->setText(QString("状态: 取消关注 @%1 失败").arg(userHandle));

    // 更新检查时间，避免重复检查
    for (int i = 0; i < m_posts.size(); ++i) {
        if (m_posts[i].authorHandle == userHandle) {
            m_posts[i].lastCheckedTime = QDateTime::currentDateTime();
        }
    }
    m_dataStorage->savePosts(m_posts);

    m_currentCheckingHandle.clear();
    m_isCheckingFollowBack = false;
    // 不再继续检查，等待下一次冷却
}

void MainWindow::appendLog(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString logLine = QString("[%1] %2").arg(timestamp, message);

    m_logTextEdit->append(logLine);

    // 限制最大100行，避免内存占用
    QTextDocument* doc = m_logTextEdit->document();
    while (doc->blockCount() > 100) {
        QTextCursor cursor(doc->begin());
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar();
    }

    // 滚动到底部
    m_logTextEdit->verticalScrollBar()->setValue(
        m_logTextEdit->verticalScrollBar()->maximum()
    );
}

void MainWindow::startSleep() {
    m_isSleeping = true;
    m_remainingSleepSeconds = 30 * 60;  // 30分钟

    // 显示休眠状态（紫色醒目提示）
    m_cooldownLabel->setStyleSheet("QLabel { background-color: #9b59b6; color: white; font-size: 18px; font-weight: bold; padding: 15px; }");
    m_cooldownLabel->setText(QString("休眠中: %1 分钟后继续 (连续失败%2次)").arg(m_remainingSleepSeconds / 60).arg(m_consecutiveFailures));
    m_cooldownLabel->setVisible(true);

    m_statusLabel->setText("状态: 连续失败，休眠30分钟...");

    // 禁用相关控件
    m_postListPanel->setEnabled(false);
    m_followedAuthorsTable->setEnabled(false);
    m_autoFollowBtn->setEnabled(false);

    // 启动休眠计时器
    m_sleepTimer->start(1000);

    qDebug() << "[INFO] Sleep started: 30 minutes";
}

void MainWindow::onSleepTick() {
    m_remainingSleepSeconds--;

    if (m_remainingSleepSeconds <= 0) {
        // 休眠结束
        m_sleepTimer->stop();
        m_isSleeping = false;
        m_consecutiveFailures = 0;  // 重置连续失败计数

        // 恢复控件
        m_postListPanel->setEnabled(true);
        m_followedAuthorsTable->setEnabled(true);
        m_autoFollowBtn->setEnabled(true);
        m_cooldownLabel->setVisible(false);

        appendLog("休眠结束，继续自动关注");
        m_statusLabel->setText("状态: 休眠结束，继续自动关注");

        qDebug() << "[INFO] Sleep ended, resuming auto-follow";

        // 继续自动关注
        if (m_isAutoFollowing) {
            processNextAutoFollow();
        }
    } else {
        // 更新休眠显示
        int minutes = m_remainingSleepSeconds / 60;
        int seconds = m_remainingSleepSeconds % 60;
        m_cooldownLabel->setText(QString("休眠中: %1:%2 后继续 (连续失败%3次)")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(m_consecutiveFailures));
    }
}

void MainWindow::onFollowersBrowserCreated() {
    qDebug() << "[INFO] Followers browser created";
    appendLog("粉丝浏览器已创建，等待页面加载...");
}

void MainWindow::onFollowersLoadFinished(bool success) {
    if (success) {
        qDebug() << "[INFO] Followers page loaded";
        // 注入粉丝监控脚本
        injectFollowersMonitorScript();

        // 如果还没启动粉丝浏览，延迟启动
        if (!m_followersSwitchTimer->isActive()) {
            // 延迟10秒后开始浏览粉丝列表
            QTimer::singleShot(10000, this, &MainWindow::startFollowersBrowsing);
        }
    }
}

void MainWindow::injectFollowersMonitorScript() {
    QString script = m_postMonitor->getFollowersMonitorScript();
    m_followersBrowser->ExecuteJavaScript(script);
    qDebug() << "[INFO] Followers monitor script injected";
}

void MainWindow::startFollowersBrowsing() {
    // 检查是否有已关注用户
    QList<Post> followedUsers;
    for (const auto& post : m_posts) {
        if (post.isFollowed && post.authorHandle != "4111y80y") {
            followedUsers.append(post);
        }
    }

    if (followedUsers.isEmpty()) {
        appendLog("没有互关用户，暂停粉丝采集");
        qDebug() << "[INFO] No followed users, pause followers browsing";
        return;
    }

    appendLog(QString("开始粉丝采集，共有 %1 个互关用户").arg(followedUsers.size()));
    qDebug() << "[INFO] Start followers browsing, total followed users:" << followedUsers.size();

    // 开始第一次切换
    onFollowersSwitchTimeout();
}

void MainWindow::onFollowersSwitchTimeout() {
    // 获取已关注用户列表
    QList<Post> followedUsers;
    for (const auto& post : m_posts) {
        if (post.isFollowed && post.authorHandle != "4111y80y") {
            followedUsers.append(post);
        }
    }

    if (followedUsers.isEmpty()) {
        appendLog("没有互关用户，暂停粉丝采集");
        m_followersSwitchTimer->stop();
        return;
    }

    // 切换到下一个用户
    m_currentFollowedUserIndex = (m_currentFollowedUserIndex + 1) % followedUsers.size();
    const Post& user = followedUsers[m_currentFollowedUserIndex];

    // 构建粉丝页面URL
    QString followersUrl = QString("https://x.com/%1/verified_followers").arg(user.authorHandle);

    appendLog(QString("切换到 @%1 的蓝V粉丝列表").arg(user.authorHandle));
    qDebug() << "[INFO] Switch to followers page:" << followersUrl;

    m_followersBrowser->LoadUrl(followersUrl);

    // 设置下一次切换时间（30-60秒随机）
    int switchInterval = 30 + (rand() % 31);
    m_followersSwitchTimer->start(switchInterval * 1000);
    qDebug() << "[INFO] Next followers switch in" << switchInterval << "seconds";
}

void MainWindow::onNewFollowersFound(const QString& jsonData) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (!doc.isArray()) {
        return;
    }

    // 固定作者的handle
    const QString pinnedAuthorHandle = "4111y80y";

    QJsonArray arr = doc.array();
    int newCount = 0;

    for (const auto& v : arr) {
        QJsonObject obj = v.toObject();
        QString userHandle = obj["authorHandle"].toString();

        // 跳过空的handle
        if (userHandle.isEmpty()) {
            continue;
        }

        // 检查是否已存在
        bool exists = false;
        for (const auto& post : m_posts) {
            if (post.authorHandle == userHandle) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            Post post;
            post.postId = "followers_" + userHandle;
            post.authorHandle = userHandle;
            post.authorName = obj["authorName"].toString();
            post.authorUrl = obj["authorUrl"].toString();
            post.content = "[粉丝采集] 来自互关用户的蓝V粉丝";
            post.matchedKeyword = "粉丝采集";
            post.collectTime = QDateTime::currentDateTime();
            post.isFollowed = false;

            m_posts.append(post);
            m_dataStorage->addPost(post);
            newCount++;
        }
    }

    if (newCount > 0) {
        // 排序：固定帖子始终第一，其他按采集时间降序
        std::sort(m_posts.begin(), m_posts.end(), [&pinnedAuthorHandle](const Post& a, const Post& b) {
            if (a.authorHandle == pinnedAuthorHandle) return true;
            if (b.authorHandle == pinnedAuthorHandle) return false;
            return a.collectTime > b.collectTime;
        });

        m_postListPanel->setPosts(m_posts);
        updateStatusBar();
        appendLog(QString("从粉丝列表采集到 %1 个新用户").arg(newCount));
        qDebug() << "[INFO] Found" << newCount << "new followers";
    }
}
