#include "DataStorage.h"
#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

DataStorage::DataStorage(QObject *parent)
    : QObject(parent), m_saveTimer(nullptr) {
  // 浏览器数据放在exe目录下（CEF需要）
  QString appDir = QCoreApplication::applicationDirPath();
  m_profilePath = appDir + "/userdata/default";
  m_scannerProfilePath = appDir + "/userdata/scanner";

  // 配置数据放在Windows标准目录
  // QStandardPaths::AppLocalDataLocation 返回:
  // C:/Users/<用户名>/AppData/Local/<应用名>
  QString appDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  m_dataPath = appDataDir + "/data";
  m_backupPath = appDataDir + "/backups";

  qDebug() << "[INFO] Data path:" << m_dataPath;
  qDebug() << "[INFO] Backup path:" << m_backupPath;
  qDebug() << "[INFO] Profile path:" << m_profilePath;
  qDebug() << "[INFO] Scanner profile path:" << m_scannerProfilePath;

  ensureDataDir();

  // 初始化延迟保存定时器
  m_saveTimer = new QTimer(this);
  m_saveTimer->setSingleShot(true);
  connect(m_saveTimer, &QTimer::timeout, this, &DataStorage::onSaveTimer);

  // 检测并迁移老版本数据
  migrateOldData();

  // 创建每日备份
  createDailyBackup();

  // 清理30天前的备份
  cleanOldBackups(30);
}

DataStorage::~DataStorage() {
  // 确保退出时保存未写入的数据
  if (m_postsDirty) {
    qDebug() << "[INFO] Saving pending posts on destruction...";
    savePostsFromCache();
  }
}

void DataStorage::ensureDataDir() {
  QDir dataDir(m_dataPath);
  if (!dataDir.exists()) {
    dataDir.mkpath(".");
  }

  QDir profileDir(m_profilePath);
  if (!profileDir.exists()) {
    profileDir.mkpath(".");
  }

  QDir scannerProfileDir(m_scannerProfilePath);
  if (!scannerProfileDir.exists()) {
    scannerProfileDir.mkpath(".");
  }

  QDir backupDir(m_backupPath);
  if (!backupDir.exists()) {
    backupDir.mkpath(".");
  }
}

QList<Keyword> DataStorage::loadKeywords() {
  QList<Keyword> keywords;

  QFile file(m_dataPath + "/keywords.json");
  if (!file.open(QIODevice::ReadOnly)) {
    // 返回默认关键词：只有"互关"
    keywords.append(Keyword("互关"));
    return keywords;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  QJsonArray arr = doc.array();
  for (const auto &v : arr) {
    keywords.append(Keyword::fromJson(v.toObject()));
  }

  if (keywords.isEmpty()) {
    keywords.append(Keyword("互关"));
  }

  return keywords;
}

void DataStorage::saveKeywords(const QList<Keyword> &keywords) {
  QJsonArray arr;
  for (const auto &kw : keywords) {
    arr.append(kw.toJson());
  }

  QFile file(m_dataPath + "/keywords.json");
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
  }
}

void DataStorage::addKeyword(const Keyword &keyword) {
  QList<Keyword> keywords = loadKeywords();
  keywords.append(keyword);
  saveKeywords(keywords);
}

void DataStorage::removeKeyword(const QString &keywordId) {
  QList<Keyword> keywords = loadKeywords();
  for (int i = 0; i < keywords.size(); ++i) {
    if (keywords[i].id == keywordId) {
      keywords.removeAt(i);
      break;
    }
  }
  saveKeywords(keywords);
}

void DataStorage::updateKeyword(const Keyword &keyword) {
  QList<Keyword> keywords = loadKeywords();
  for (int i = 0; i < keywords.size(); ++i) {
    if (keywords[i].id == keyword.id) {
      keywords[i] = keyword;
      break;
    }
  }
  saveKeywords(keywords);
}

void DataStorage::loadPostsToCache() {
  if (m_postsCacheLoaded) {
    return;
  }

  m_postsCache.clear();
  m_postIdIndex.clear();

  QFile file(m_dataPath + "/posts.json");
  if (!file.open(QIODevice::ReadOnly)) {
    m_postsCacheLoaded = true;
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  QJsonArray arr = doc.array();
  for (const auto &v : arr) {
    Post post = Post::fromJson(v.toObject());
    m_postsCache.append(post);
    m_postIdIndex.insert(post.postId);
  }

  m_postsCacheLoaded = true;
  qDebug() << "[INFO] Posts cache loaded:" << m_postsCache.size() << "posts";
}

QList<Post> DataStorage::loadPosts() {
  loadPostsToCache();
  return m_postsCache;
}

void DataStorage::savePosts(const QList<Post> &posts) {
  // 更新缓存
  m_postsCache = posts;
  m_postIdIndex.clear();
  for (const auto &post : posts) {
    m_postIdIndex.insert(post.postId);
  }
  m_postsCacheLoaded = true;
  m_postsDirty = true;

  // 调度延迟保存
  scheduleSave();
}

void DataStorage::addPost(const Post &post) {
  loadPostsToCache();

  // 使用索引快速检查是否已存在
  if (m_postIdIndex.contains(post.postId)) {
    return;
  }

  // 添加到缓存
  m_postsCache.prepend(post); // 新帖子放在最前面
  m_postIdIndex.insert(post.postId);
  m_postsDirty = true;

  // 调度延迟保存
  scheduleSave();
}

void DataStorage::updatePost(const Post &post) {
  loadPostsToCache();

  for (int i = 0; i < m_postsCache.size(); ++i) {
    if (m_postsCache[i].postId == post.postId) {
      m_postsCache[i] = post;
      m_postsDirty = true;
      scheduleSave();
      break;
    }
  }
}

bool DataStorage::postExists(const QString &postId) {
  loadPostsToCache();
  // O(1) 查询
  return m_postIdIndex.contains(postId);
}

void DataStorage::scheduleSave() {
  // 重置定时器，延迟5秒后保存
  if (m_saveTimer) {
    m_saveTimer->start(SAVE_DELAY_MS);
  }
}

void DataStorage::onSaveTimer() {
  if (m_postsDirty) {
    savePostsFromCache();
  }
}

void DataStorage::flushPosts() {
  // 停止定时器
  if (m_saveTimer) {
    m_saveTimer->stop();
  }
  // 立即保存
  if (m_postsDirty) {
    savePostsFromCache();
  }
}

void DataStorage::savePostsFromCache() {
  if (!m_postsDirty) {
    return;
  }

  QString filePath = m_dataPath + "/posts.json";
  QString tmpPath = filePath + ".tmp";
  QString bakPath = filePath + ".bak";

  // 1. 写入临时文件
  QJsonArray arr;
  for (const auto &post : m_postsCache) {
    arr.append(post.toJson());
  }

  QFile tmpFile(tmpPath);
  if (!tmpFile.open(QIODevice::WriteOnly)) {
    qDebug() << "[ERROR] Failed to create temp file:" << tmpPath;
    return;
  }
  tmpFile.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
  tmpFile.close();

  // 2. 备份原文件（如果存在）
  if (QFile::exists(filePath)) {
    if (QFile::exists(bakPath)) {
      QFile::remove(bakPath);
    }
    if (!QFile::rename(filePath, bakPath)) {
      qDebug() << "[ERROR] Failed to backup posts.json";
      QFile::remove(tmpPath);
      return;
    }
  }

  // 3. 原子替换：tmp -> json
  if (!QFile::rename(tmpPath, filePath)) {
    qDebug() << "[ERROR] Failed to rename temp file to posts.json";
    // 尝试恢复备份
    if (QFile::exists(bakPath)) {
      QFile::rename(bakPath, filePath);
    }
    return;
  }

  m_postsDirty = false;
  qDebug() << "[INFO] Posts saved to disk:" << m_postsCache.size() << "posts";
  emit postsSaved();
}

QJsonObject DataStorage::loadConfig() {
  QFile file(m_dataPath + "/config.json");
  if (!file.open(QIODevice::ReadOnly)) {
    return QJsonObject();
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  return doc.object();
}

void DataStorage::saveConfig(const QJsonObject &config) {
  QFile file(m_dataPath + "/config.json");
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    file.close();
  }
}

void DataStorage::migrateOldData() {
  // 老版本数据可能存在的位置
  QStringList oldPaths = {
      QCoreApplication::applicationDirPath() + "/data", // exe目录下
      "E:/xfollowing/data"                              // 硬编码的E盘路径
  };

  QStringList dataFiles = {"posts.json", "keywords.json", "config.json"};

  for (const QString &oldPath : oldPaths) {
    QDir oldDir(oldPath);
    if (!oldDir.exists()) {
      continue;
    }

    qDebug() << "[Migration] Checking old data path:" << oldPath;

    for (const QString &fileName : dataFiles) {
      QString oldFile = oldPath + "/" + fileName;
      QString newFile = m_dataPath + "/" + fileName;

      if (!QFile::exists(oldFile)) {
        continue;
      }

      QFileInfo oldInfo(oldFile);
      QFileInfo newInfo(newFile);

      // 如果新位置没有该文件，或老文件更新，则迁移
      if (!newInfo.exists() ||
          oldInfo.lastModified() > newInfo.lastModified()) {
        // 备份老文件（添加.migrated后缀）
        QString backupFile = oldFile + ".migrated";
        if (!QFile::exists(backupFile)) {
          QFile::copy(oldFile, backupFile);
        }

        // 复制到新位置
        if (QFile::exists(newFile)) {
          QFile::remove(newFile);
        }
        if (QFile::copy(oldFile, newFile)) {
          qDebug() << "[Migration] Migrated" << fileName << "from" << oldPath;
        } else {
          qDebug() << "[Migration] Failed to migrate" << fileName << "from"
                   << oldPath;
        }
      }
    }
  }
}

void DataStorage::createDailyBackup() {
  QString today = QDate::currentDate().toString("yyyy-MM-dd");
  QString todayBackupDir = m_backupPath + "/" + today;

  QDir dir;
  // 如果今天的备份目录已存在，跳过
  if (dir.exists(todayBackupDir)) {
    qDebug() << "[Backup] Today's backup already exists:" << today;
    return;
  }

  // 创建今天的备份目录
  if (!dir.mkpath(todayBackupDir)) {
    qDebug() << "[Backup] Failed to create backup directory:" << todayBackupDir;
    return;
  }

  // 复制当前数据文件到备份目录
  QStringList dataFiles = {"posts.json", "keywords.json", "config.json"};
  int copiedCount = 0;

  for (const QString &fileName : dataFiles) {
    QString srcFile = m_dataPath + "/" + fileName;
    QString dstFile = todayBackupDir + "/" + fileName;

    if (QFile::exists(srcFile)) {
      if (QFile::copy(srcFile, dstFile)) {
        copiedCount++;
      }
    }
  }

  if (copiedCount > 0) {
    qDebug() << "[Backup] Created daily backup:" << today << "(" << copiedCount
             << "files)";
  }
}

void DataStorage::cleanOldBackups(int keepDays) {
  QDir backupDir(m_backupPath);
  if (!backupDir.exists()) {
    return;
  }

  QStringList dirs = backupDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  QDate cutoff = QDate::currentDate().addDays(-keepDays);
  int removedCount = 0;

  for (const QString &dirName : dirs) {
    QDate dirDate = QDate::fromString(dirName, "yyyy-MM-dd");
    if (dirDate.isValid() && dirDate < cutoff) {
      QString fullPath = m_backupPath + "/" + dirName;
      QDir dirToRemove(fullPath);
      if (dirToRemove.removeRecursively()) {
        removedCount++;
        qDebug() << "[Backup] Removed old backup:" << dirName;
      }
    }
  }

  if (removedCount > 0) {
    qDebug() << "[Backup] Cleaned" << removedCount << "old backups (older than"
             << keepDays << "days)";
  }
}

QSet<QString> DataStorage::loadUsedFollowBackHandles() {
  QSet<QString> handles;
  QString filePath = m_dataPath + "/followback_used.json";
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return handles;
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (doc.isArray()) {
    QJsonArray arr = doc.array();
    for (const auto &v : arr) {
      handles.insert(v.toString());
    }
  }
  return handles;
}

void DataStorage::saveUsedFollowBackHandles(const QSet<QString> &handles) {
  QString filePath = m_dataPath + "/followback_used.json";
  QJsonArray arr;
  for (const auto &h : handles) {
    arr.append(h);
  }
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(arr).toJson());
    file.close();
  }
}

QJsonArray DataStorage::loadGeneratedTweets() {
  QString filePath = m_dataPath + "/generated_tweets.json";
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return QJsonArray();
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (doc.isArray()) {
    return doc.array();
  }
  return QJsonArray();
}

void DataStorage::saveGeneratedTweets(const QJsonArray &tweets) {
  QString filePath = m_dataPath + "/generated_tweets.json";
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(tweets).toJson());
    file.close();
  }
}

QJsonArray DataStorage::loadTweetTemplates() {
  // 先在数据目录查找，再在exe目录查找
  QStringList searchPaths = {
      m_dataPath + "/tweet_templates.json",
      QCoreApplication::applicationDirPath() + "/data/tweet_templates.json",
      QCoreApplication::applicationDirPath() + "/tweet_templates.json"};
  for (const auto &filePath : searchPaths) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
      continue;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isArray()) {
      qDebug() << "[INFO] Loaded" << doc.array().size()
               << "tweet templates from" << filePath;
      return doc.array();
    }
  }
  qDebug() << "[WARN] tweet_templates.json not found in any search path";
  return QJsonArray();
}

QJsonArray DataStorage::loadPendingFollowBackUsers() {
  QString filePath = m_dataPath + "/pending_followback_users.json";
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return QJsonArray();
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (doc.isArray()) {
    return doc.array();
  }
  return QJsonArray();
}

void DataStorage::savePendingFollowBackUsers(const QJsonArray &users) {
  QString filePath = m_dataPath + "/pending_followback_users.json";
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(users).toJson());
    file.close();
  }
}
