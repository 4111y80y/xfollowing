#ifndef DATASTORAGE_H
#define DATASTORAGE_H

#include "Keyword.h"
#include "Post.h"
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

class DataStorage : public QObject {
  Q_OBJECT

public:
  explicit DataStorage(QObject *parent = nullptr);
  ~DataStorage();

  // 关键词管理
  QList<Keyword> loadKeywords();
  void saveKeywords(const QList<Keyword> &keywords);
  void addKeyword(const Keyword &keyword);
  void removeKeyword(const QString &keywordId);
  void updateKeyword(const Keyword &keyword);

  // 帖子管理
  QList<Post> loadPosts();
  void savePosts(const QList<Post> &posts);
  void addPost(const Post &post);
  void updatePost(const Post &post);
  bool postExists(const QString &postId);
  void flushPosts(); // 强制保存到磁盘

  // 回关追踪数据管理
  QSet<QString> loadUsedFollowBackHandles();
  void saveUsedFollowBackHandles(const QSet<QString> &handles);
  QJsonArray loadGeneratedTweets(); // [{text, status}]
  void saveGeneratedTweets(const QJsonArray &tweets);
  QJsonArray loadTweetTemplates();         // [{header, footer}]
  QJsonArray loadPendingFollowBackUsers(); // [{handle, responseSeconds, ...}]
  void savePendingFollowBackUsers(const QJsonArray &users);

  // 配置管理
  QJsonObject loadConfig();
  void saveConfig(const QJsonObject &config);

  // 获取存储路径
  QString getDataPath() const { return m_dataPath; }
  QString getProfilePath() const { return m_profilePath; }
  QString getScannerProfilePath() const { return m_scannerProfilePath; }
  QString getBackupPath() const { return m_backupPath; }

  // 数据迁移和备份
  void migrateOldData();                   // 迁移老版本数据
  void createDailyBackup();                // 创建每日备份
  void cleanOldBackups(int keepDays = 30); // 清理超过指定天数的备份

signals:
  void postsSaved();

private slots:
  void onSaveTimer();

private:
  void ensureDataDir();
  void loadPostsToCache();
  void savePostsFromCache();
  void scheduleSave();

  QString m_dataPath;           // 数据目录 (%LOCALAPPDATA%/xfollowing/data)
  QString m_profilePath;        // 浏览器配置目录 (exe目录/userdata/default)
  QString m_scannerProfilePath; // 扫描浏览器配置目录 (exe目录/userdata/scanner)
  QString m_backupPath;         // 备份目录 (%LOCALAPPDATA%/xfollowing/backups)

  // 帖子缓存相关
  QList<Post> m_postsCache;              // 帖子列表缓存
  QSet<QString> m_postIdIndex;           // postId 索引，O(1)查询
  bool m_postsCacheLoaded = false;       // 缓存是否已加载
  bool m_postsDirty = false;             // 是否有未保存的修改
  QTimer *m_saveTimer;                   // 延迟保存定时器
  static const int SAVE_DELAY_MS = 5000; // 5秒延迟
};

#endif // DATASTORAGE_H
