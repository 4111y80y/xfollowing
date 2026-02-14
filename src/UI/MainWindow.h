#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Data/Keyword.h"
#include "Data/Post.h"
#include <QCheckBox>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>

class BrowserWidget;
class KeywordPanel;
class PostListPanel;
class DataStorage;
class PostMonitor;
class AutoFollower;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void closeEvent(QCloseEvent *event) override;
  void timerEvent(QTimerEvent *event) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void onSearchBrowserCreated();
  void onUserBrowserCreated();
  void onSearchLoadFinished(bool success);
  void onUserLoadFinished(bool success);
  void onPostClicked(const Post &post);
  void onNewPostsFound(const QString &jsonData);
  void onFollowSuccess(const QString &userHandle);
  void onAlreadyFollowing(const QString &userHandle);
  void onFollowFailed(const QString &userHandle);
  void onAccountSuspended(const QString &userHandle);
  void onHideFollowedChanged(bool checked);
  void onKeywordsChanged();
  void onCooldownTick();
  void onFollowedAuthorDoubleClicked(int row, int column);
  void onKeywordDoubleClicked(const QString &keyword);
  void onAutoFollowToggled();
  void processNextAutoFollow();
  void onAutoRefreshTimeout();
  // 回关检查槽函数
  void onCheckFollowsBack(const QString &userHandle);
  void onCheckNotFollowBack(const QString &userHandle);
  void onCheckSuspended(const QString &userHandle);
  void onCheckNotFollowing(const QString &userHandle);
  void onUnfollowSuccess(const QString &userHandle);
  void onUnfollowFailed(const QString &userHandle);
  void onSleepTick();    // 休眠计时器
  void onWatchdogTick(); // 自动关注看门狗
  // 粉丝浏览器槽函数
  void onFollowersBrowserCreated();
  void onFollowersLoadFinished(bool success);
  void onNewFollowersFound(const QString &jsonData);
  void onFollowersSwitchTimeout();
  // 已关注用户分页槽函数
  void onFollowedFirstPage();
  void onFollowedPrevPage();
  void onFollowedNextPage();
  void onFollowedLastPage();
  // 登录状态检测
  void onUserLoggedIn();
  // 回关探测浏览器槽函数
  void onFollowBackDetectBrowserCreated();
  void onFollowBackDetectLoadFinished(bool success);
  void onFollowBackDetectRefresh();
  void onNewFollowBackDetected(const QString &jsonData);
  // 生成帖子列表交互
  void onGeneratedTweetClicked(int row);
  void onTweetListContextMenu(const QPoint &pos);

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
  void startFollowBackCheck();            // 开始回关检查
  void checkNextFollowBack();             // 检查下一个用户
  void appendLog(const QString &message); // 追加日志
  void startSleep();                      // 开始休眠
  void injectFollowersMonitorScript();    // 注入粉丝监控脚本
  void startFollowersBrowsing();          // 开始浏览粉丝
  void renderFollowedPage();              // 渲染已关注用户当前页
  void updateFollowedPageInfo();          // 更新分页信息
  void updateFollowersBrowserState();     // 更新粉丝浏览器状态
  int countPendingKeywordAccounts();      // 统计待关注的关键词账号数量
  void injectFollowBackDetectScript();    // 注入回关探测脚本
  void tryGenerateFollowBackTweet();      // 尝试生成回关帖子
  void addGeneratedTweet(const QString &tweetText); // 添加生成的帖子
  void updateTweetListItem(int row);                // 更新列表项显示
  void refreshTweetList();                          // 刷新帖子列表
  QString formatDuration(qint64 seconds);           // 格式化时间差

  // UI Components - 三栏布局
  QSplitter *m_mainSplitter;

  // 左侧分割器（搜索浏览器 + 粉丝浏览器）
  QSplitter *m_leftSplitter;

  // 左侧浏览器 - 搜索页
  BrowserWidget *m_searchBrowser;

  // 左侧浏览器 - 粉丝页
  BrowserWidget *m_followersBrowser;
  bool m_followersBrowserInitialized;
  QLabel *m_followersPausedLabel; // 粉丝浏览器暂停提示

  // 中间面板
  QWidget *m_centerPanel;
  KeywordPanel *m_keywordPanel;
  QTabWidget *m_tabWidget;
  PostListPanel *m_postListPanel;
  QTableWidget *m_followedAuthorsTable;
  QCheckBox *m_hideFollowedCheckBox;
  QSpinBox *m_cooldownMinSpinBox;
  QSpinBox *m_cooldownMaxSpinBox;
  QPushButton *m_autoFollowBtn;
  QSpinBox *m_unfollowDaysSpinBox; // 取关天数设置
  QSpinBox *m_checkCountSpinBox;   // 每轮回关检查数量
  QSpinBox *m_recheckDaysSpinBox;  // 回关检查间隔天数

  // 右侧浏览器 - 用户页
  QWidget *m_rightPanel;
  QLabel *m_cooldownLabel;
  QLabel *m_hintLabel;
  BrowserWidget *m_userBrowser;
  QTextEdit *m_logTextEdit; // 日志信息框

  // 第4列 - 回关探测浏览器
  BrowserWidget *m_followBackDetectBrowser;
  bool m_followBackDetectBrowserInitialized;
  QTimer *m_followBackDetectTimer; // 定时刷新粉丝页

  // 第5列 - 生成帖子面板
  QWidget *m_tweetGenPanel;
  QListWidget *m_generatedTweetsList; // 帖子列表
  QTextEdit *m_tweetPreviewEdit;      // 帖子预览

  // 状态栏
  QLabel *m_statusLabel;
  QLabel *m_statsLabel;

  // 核心模块
  DataStorage *m_dataStorage;
  PostMonitor *m_postMonitor;
  AutoFollower *m_autoFollower;

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
  QTimer *m_cooldownTimer;
  int m_cooldownMinSeconds;
  int m_cooldownMaxSeconds;
  int m_remainingCooldown;
  bool m_isCooldownActive;

  // 自动批量关注
  bool m_isAutoFollowing;

  // 自动刷新搜索页
  QTimer *m_autoRefreshTimer;
  int m_currentKeywordIndex; // 当前关键词索引

  // 粉丝采集
  QTimer *m_followersSwitchTimer;
  int m_currentFollowedUserIndex; // 当前正在浏览的互关用户索引

  // 回关检查
  bool m_isCheckingFollowBack;     // 是否正在检查回关
  QString m_currentCheckingHandle; // 当前正在检查的用户
  int m_followBackCheckCount;      // 本轮已检查的用户数

  // 连续失败休眠
  int m_consecutiveFailures;   // 连续失败次数
  bool m_isSleeping;           // 是否在休眠
  int m_remainingSleepSeconds; // 剩余休眠秒数
  QTimer *m_sleepTimer;        // 休眠计时器

  // 已关注用户分页
  QList<Post> m_followedPosts; // 已关注用户缓存
  int m_followedCurrentPage = 0;
  int m_followedPageSize = 100;
  int m_followedTotalPages = 0;

  // 自动关注看门狗
  QTimer *m_autoFollowWatchdog; // 看门狗定时器
  int m_watchdogCounter = 0;    // 看门狗计数器

  // 分页控件
  QLabel *m_followedPageLabel;
  QPushButton *m_followedFirstBtn;
  QPushButton *m_followedPrevBtn;
  QPushButton *m_followedNextBtn;
  QPushButton *m_followedLastBtn;

  // 回关追踪数据
  QList<QJsonObject> m_followBackUsers;    // 已回关用户（尚未生成帖子的）
  QSet<QString> m_usedFollowBackHandles;   // 已生成过帖子的用户handle（去重）
  QJsonArray m_generatedTweets;            // 已生成的帖子 [{text, status}]
  QSet<QString> m_detectedFollowerHandles; // 已检测到的粉丝handle集合
  QJsonArray m_tweetTemplates;             // 帖子模板 [{header, footer}]
};

#endif // MAINWINDOW_H
