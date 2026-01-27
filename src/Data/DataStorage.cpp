#include "DataStorage.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>

DataStorage::DataStorage(QObject* parent)
    : QObject(parent) {
    // 数据文件放在 E 盘，避免 build 目录被误删导致数据丢失
    m_dataPath = "E:/xfollowing/data";
    // 用户配置放在 exe 目录下（CEF 需要）
    QString appDir = QCoreApplication::applicationDirPath();
    m_profilePath = appDir + "/userdata/default";

    ensureDataDir();
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
