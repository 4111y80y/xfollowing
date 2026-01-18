#ifndef DATASTORAGE_H
#define DATASTORAGE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include "Post.h"
#include "Keyword.h"

class DataStorage : public QObject {
    Q_OBJECT

public:
    explicit DataStorage(QObject* parent = nullptr);

    // 关键词管理
    QList<Keyword> loadKeywords();
    void saveKeywords(const QList<Keyword>& keywords);
    void addKeyword(const Keyword& keyword);
    void removeKeyword(const QString& keywordId);
    void updateKeyword(const Keyword& keyword);

    // 帖子管理
    QList<Post> loadPosts();
    void savePosts(const QList<Post>& posts);
    void addPost(const Post& post);
    void updatePost(const Post& post);
    bool postExists(const QString& postId);

    // 配置管理
    QJsonObject loadConfig();
    void saveConfig(const QJsonObject& config);

    // 获取存储路径
    QString getDataPath() const { return m_dataPath; }
    QString getProfilePath() const { return m_profilePath; }

private:
    void ensureDataDir();

    QString m_dataPath;
    QString m_profilePath;
};

#endif // DATASTORAGE_H
