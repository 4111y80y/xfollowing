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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mainSplitter(nullptr)
    , m_searchBrowser(nullptr)
    , m_centerPanel(nullptr)
    , m_keywordPanel(nullptr)
    , m_postListPanel(nullptr)
    , m_hideFollowedCheckBox(nullptr)
    , m_userBrowser(nullptr)
    , m_statusLabel(nullptr)
    , m_dataStorage(nullptr)
    , m_postMonitor(nullptr)
    , m_autoFollower(nullptr)
    , m_cefTimerId(0)
    , m_searchBrowserInitialized(false)
    , m_userBrowserInitialized(false) {

    setWindowTitle("X互关宝 - X.com互关粉丝助手");
    resize(1600, 900);

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

    // 帖子列表区域
    QGroupBox* postGroup = new QGroupBox("监控到的帖子", m_centerPanel);
    QVBoxLayout* postLayout = new QVBoxLayout(postGroup);

    m_postListPanel = new PostListPanel(postGroup);
    m_postListPanel->setPosts(m_posts);
    postLayout->addWidget(m_postListPanel);

    // 隐藏已关注开关
    m_hideFollowedCheckBox = new QCheckBox("隐藏已关注的帖子", postGroup);
    postLayout->addWidget(m_hideFollowedCheckBox);

    centerLayout->addWidget(postGroup, 1);

    // 右侧 - 用户页浏览器
    m_userBrowser = new BrowserWidget(m_mainSplitter);
    m_userBrowser->setMinimumWidth(500);

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
    updateStatusBar();

    m_statusLabel->setText(QString("状态: 成功关注 @%1").arg(m_currentFollowingHandle));
    m_currentFollowingHandle.clear();
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
