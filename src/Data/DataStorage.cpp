#include "DataStorage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>
#include <QDate>

DataStorage::DataStorage(QObject* parent)
    : QObject(parent) {
    // 浏览器数据放在exe目录下（CEF需要）
    QString appDir = QCoreApplication::applicationDirPath();
    m_profilePath = appDir + "/userdata/default";

    // 配置数据放在Windows标准目录
    // QStandardPaths::AppLocalDataLocation 返回: C:/Users/<用户名>/AppData/Local/<应用名>
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    m_dataPath = appDataDir + "/data";
    m_backupPath = appDataDir + "/backups";

    qDebug() << "[INFO] Data path:" << m_dataPath;
    qDebug() << "[INFO] Backup path:" << m_backupPath;
    qDebug() << "[INFO] Profile path:" << m_profilePath;

    ensureDataDir();

    // 检测并迁移老版本数据
    migrateOldData();

    // 创建每日备份
    createDailyBackup();

    // 清理30天前的备份
    cleanOldBackups(30);
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
    for (const auto& v : arr) {
        keywords.append(Keyword::fromJson(v.toObject()));
    }

    if (keywords.isEmpty()) {
        keywords.append(Keyword("互关"));
    }

    return keywords;
}

void DataStorage::saveKeywords(const QList<Keyword>& keywords) {
    QJsonArray arr;
    for (const auto& kw : keywords) {
        arr.append(kw.toJson());
    }

    QFile file(m_dataPath + "/keywords.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void DataStorage::addKeyword(const Keyword& keyword) {
    QList<Keyword> keywords = loadKeywords();
    keywords.append(keyword);
    saveKeywords(keywords);
}

void DataStorage::removeKeyword(const QString& keywordId) {
    QList<Keyword> keywords = loadKeywords();
    for (int i = 0; i < keywords.size(); ++i) {
        if (keywords[i].id == keywordId) {
            keywords.removeAt(i);
            break;
        }
    }
    saveKeywords(keywords);
}

void DataStorage::updateKeyword(const Keyword& keyword) {
    QList<Keyword> keywords = loadKeywords();
    for (int i = 0; i < keywords.size(); ++i) {
        if (keywords[i].id == keyword.id) {
            keywords[i] = keyword;
            break;
        }
    }
    saveKeywords(keywords);
}

QList<Post> DataStorage::loadPosts() {
    QList<Post> posts;

    QFile file(m_dataPath + "/posts.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return posts;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonArray arr = doc.array();
    for (const auto& v : arr) {
        posts.append(Post::fromJson(v.toObject()));
    }

    return posts;
}

void DataStorage::savePosts(const QList<Post>& posts) {
    QJsonArray arr;
    for (const auto& post : posts) {
        arr.append(post.toJson());
    }

    QFile file(m_dataPath + "/posts.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void DataStorage::addPost(const Post& post) {
    QList<Post> posts = loadPosts();

    // 检查是否已存在
    for (const auto& p : posts) {
        if (p.postId == post.postId) {
            return;
        }
    }

    posts.prepend(post);  // 新帖子放在最前面
    savePosts(posts);
}

void DataStorage::updatePost(const Post& post) {
    QList<Post> posts = loadPosts();
    for (int i = 0; i < posts.size(); ++i) {
        if (posts[i].postId == post.postId) {
            posts[i] = post;
            break;
        }
    }
    savePosts(posts);
}

bool DataStorage::postExists(const QString& postId) {
    QList<Post> posts = loadPosts();
    for (const auto& p : posts) {
        if (p.postId == postId) {
            return true;
        }
    }
    return false;
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

void DataStorage::saveConfig(const QJsonObject& config) {
    QFile file(m_dataPath + "/config.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void DataStorage::migrateOldData() {
    // 老版本数据可能存在的位置
    QStringList oldPaths = {
        QCoreApplication::applicationDirPath() + "/data",  // exe目录下
        "E:/xfollowing/data"                                // 硬编码的E盘路径
    };

    QStringList dataFiles = {"posts.json", "keywords.json", "config.json"};

    for (const QString& oldPath : oldPaths) {
        QDir oldDir(oldPath);
        if (!oldDir.exists()) {
            continue;
        }

        qDebug() << "[Migration] Checking old data path:" << oldPath;

        for (const QString& fileName : dataFiles) {
            QString oldFile = oldPath + "/" + fileName;
            QString newFile = m_dataPath + "/" + fileName;

            if (!QFile::exists(oldFile)) {
                continue;
            }

            QFileInfo oldInfo(oldFile);
            QFileInfo newInfo(newFile);

            // 如果新位置没有该文件，或老文件更新，则迁移
            if (!newInfo.exists() || oldInfo.lastModified() > newInfo.lastModified()) {
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
                    qDebug() << "[Migration] Failed to migrate" << fileName << "from" << oldPath;
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

    for (const QString& fileName : dataFiles) {
        QString srcFile = m_dataPath + "/" + fileName;
        QString dstFile = todayBackupDir + "/" + fileName;

        if (QFile::exists(srcFile)) {
            if (QFile::copy(srcFile, dstFile)) {
                copiedCount++;
            }
        }
    }

    if (copiedCount > 0) {
        qDebug() << "[Backup] Created daily backup:" << today << "(" << copiedCount << "files)";
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

    for (const QString& dirName : dirs) {
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
        qDebug() << "[Backup] Cleaned" << removedCount << "old backups (older than" << keepDays << "days)";
    }
}
