#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QTabWidget>
#include <QTableWidget>
#include <QList>
#include <QTextEdit>
#include "Data/Post.h"
#include "Data/Keyword.h"

class BrowserWidget;
class KeywordPanel;
class PostListPanel;
class DataStorage;
class PostMonitor;
class AutoFollower;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onSearchBrowserCreated();
    void onUserBrowserCreated();
    void onSearchLoadFinished(bool success);
    void onUserLoadFinished(bool success);
    void onPostClicked(const Post& post);
    void onNewPostsFound(const QString& jsonData);
    void onFollowSuccess(const QString& userHandle);
    void onAlreadyFollowing(const QString& userHandle);
    void onFollowFailed(const QString& userHandle);
    void onAccountSuspended(const QString& userHandle);
    void onHideFollowedChanged(bool checked);
    void onKeywordsChanged();
    void onCooldownTick();
    void onFollowedAuthorDoubleClicked(int row, int column);
    void onKeywordDoubleClicked(const QString& keyword);
    void onAutoFollowToggled();
    void processNextAutoFollow();
    void onAutoRefreshTimeout();
    // 回关检查槽函数
    void onCheckFollowsBack(const QString& userHandle);
    void onCheckNotFollowBack(const QString& userHandle);
    void onCheckSuspended(const QString& userHandle);
    void onCheckNotFollowing(const QString& userHandle);
    void onUnfollowSuccess(const QString& userHandle);
    void onUnfollowFailed(const QString& userHandle);
    void onSleepTick();  // 休眠计时器
    // 粉丝浏览器槽函数
    void onFollowersBrowserCreated();
    void onFollowersLoadFinished(bool success);
    void onNewFollowersFound(const QString& jsonData);
    void onFollowersSwitchTimeout();
    // 已关注用户分页槽函数
    void onFollowedFirstPage();
    void onFollowedPrevPage();
    void onFollowedNextPage();
    void onFollowedLastPage();
    // 登录状态检测
    void onUserLoggedIn();

private:
    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateStatusBar();
    void injectMonitorScript();
    void addPinnedAuthorPost();
    void startCooldown();
    void updateCooldownDisplay();
    void updateFollowedAuthorsTable();
    void startFollowBackCheck();  // 开始回关检查
    void checkNextFollowBack();   // 检查下一个用户
    void appendLog(const QString& message);  // 追加日志
    void startSleep();            // 开始休眠
    void injectFollowersMonitorScript();  // 注入粉丝监控脚本
    void startFollowersBrowsing();        // 开始浏览粉丝
    void renderFollowedPage();            // 渲染已关注用户当前页
    void updateFollowedPageInfo();        // 更新分页信息

    // UI Components - 三栏布局
    QSplitter* m_mainSplitter;

    // 左侧分割器（搜索浏览器 + 粉丝浏览器）
    QSplitter* m_leftSplitter;

    // 左侧浏览器 - 搜索页
    BrowserWidget* m_searchBrowser;

    // 左侧浏览器 - 粉丝页
    BrowserWidget* m_followersBrowser;
    bool m_followersBrowserInitialized;

    // 中间面板
    QWidget* m_centerPanel;
    KeywordPanel* m_keywordPanel;
    QTabWidget* m_tabWidget;
    PostListPanel* m_postListPanel;
    QTableWidget* m_followedAuthorsTable;
    QCheckBox* m_hideFollowedCheckBox;
    QSpinBox* m_cooldownMinSpinBox;
    QSpinBox* m_cooldownMaxSpinBox;
    QPushButton* m_autoFollowBtn;
    QSpinBox* m_unfollowDaysSpinBox;  // 取关天数设置
    QSpinBox* m_checkCountSpinBox;    // 每轮回关检查数量
    QSpinBox* m_recheckDaysSpinBox;   // 回关检查间隔天数

    // 右侧浏览器 - 用户页
    QWidget* m_rightPanel;
    QLabel* m_cooldownLabel;
    QLabel* m_hintLabel;
    BrowserWidget* m_userBrowser;
    QTextEdit* m_logTextEdit;  // 日志信息框

    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_statsLabel;

    // 核心模块
    DataStorage* m_dataStorage;
    PostMonitor* m_postMonitor;
    AutoFollower* m_autoFollower;

    // 数据
    QList<Post> m_posts;
    QList<Keyword> m_keywords;

    // CEF timer
    int m_cefTimerId;
    bool m_searchBrowserInitialized;
    bool m_userBrowserInitialized;

    // 当前正在关注的用户
    QString m_currentFollowingHandle;

    // 冷却时间
    QTimer* m_cooldownTimer;
    int m_cooldownMinSeconds;
    int m_cooldownMaxSeconds;
    int m_remainingCooldown;
    bool m_isCooldownActive;

    // 自动批量关注
    bool m_isAutoFollowing;

    // 自动刷新搜索页
    QTimer* m_autoRefreshTimer;
    int m_currentKeywordIndex;  // 当前关键词索引

    // 粉丝采集
    QTimer* m_followersSwitchTimer;
    int m_currentFollowedUserIndex;  // 当前正在浏览的互关用户索引

    // 回关检查
    bool m_isCheckingFollowBack;       // 是否正在检查回关
    QString m_currentCheckingHandle;   // 当前正在检查的用户
    int m_followBackCheckCount;        // 本轮已检查的用户数

    // 连续失败休眠
    int m_consecutiveFailures;         // 连续失败次数
    bool m_isSleeping;                 // 是否在休眠
    int m_remainingSleepSeconds;       // 剩余休眠秒数
    QTimer* m_sleepTimer;              // 休眠计时器

    // 已关注用户分页
    QList<Post> m_followedPosts;       // 已关注用户缓存
    int m_followedCurrentPage = 0;
    int m_followedPageSize = 100;
    int m_followedTotalPages = 0;

    // 分页控件
    QLabel* m_followedPageLabel;
    QPushButton* m_followedFirstBtn;
    QPushButton* m_followedPrevBtn;
    QPushButton* m_followedNextBtn;
    QPushButton* m_followedLastBtn;
};

#endif // MAINWINDOW_H
