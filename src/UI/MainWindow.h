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
    void onHideFollowedChanged(bool checked);
    void onKeywordsChanged();
    void onCooldownTick();
    void onFollowedAuthorDoubleClicked(int row, int column);
    void onKeywordDoubleClicked(const QString& keyword);
    void onAutoFollowToggled();
    void processNextAutoFollow();

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

    // UI Components - 三栏布局
    QSplitter* m_mainSplitter;

    // 左侧浏览器 - 搜索页
    BrowserWidget* m_searchBrowser;

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

    // 右侧浏览器 - 用户页
    QWidget* m_rightPanel;
    QLabel* m_cooldownLabel;
    QLabel* m_hintLabel;
    BrowserWidget* m_userBrowser;

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
};

#endif // MAINWINDOW_H
