#ifndef POST_H
#define POST_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

struct Post {
    QString postId;            // 帖子唯一ID
    QString authorName;        // 作者显示名
    QString authorHandle;      // 作者@handle
    QString authorUrl;         // 作者主页URL
    QString content;           // 帖子内容
    QString postUrl;           // 帖子URL
    QDateTime postTime;        // 帖子发布时间
    QDateTime collectTime;     // 采集时间
    QString matchedKeyword;    // 匹配的关键词
    bool isFollowed = false;   // 是否已关注
    bool isHidden = false;     // 是否隐藏
    QDateTime followTime;      // 关注时间
    QDateTime lastCheckedTime; // 上次回关检查时间

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["postId"] = postId;
        obj["authorName"] = authorName;
        obj["authorHandle"] = authorHandle;
        obj["authorUrl"] = authorUrl;
        obj["content"] = content;
        obj["postUrl"] = postUrl;
        obj["postTime"] = postTime.toString(Qt::ISODate);
        obj["collectTime"] = collectTime.toString(Qt::ISODate);
        obj["matchedKeyword"] = matchedKeyword;
        obj["isFollowed"] = isFollowed;
        obj["isHidden"] = isHidden;
        obj["followTime"] = followTime.toString(Qt::ISODate);
        obj["lastCheckedTime"] = lastCheckedTime.toString(Qt::ISODate);
        return obj;
    }

    static Post fromJson(const QJsonObject& obj) {
        Post post;
        post.postId = obj["postId"].toString();
        post.authorName = obj["authorName"].toString();
        post.authorHandle = obj["authorHandle"].toString();
        post.authorUrl = obj["authorUrl"].toString();
        post.content = obj["content"].toString();
        post.postUrl = obj["postUrl"].toString();
        post.postTime = QDateTime::fromString(obj["postTime"].toString(), Qt::ISODate);
        post.collectTime = QDateTime::fromString(obj["collectTime"].toString(), Qt::ISODate);
        post.matchedKeyword = obj["matchedKeyword"].toString();
        post.isFollowed = obj["isFollowed"].toBool();
        post.isHidden = obj["isHidden"].toBool();
        post.followTime = QDateTime::fromString(obj["followTime"].toString(), Qt::ISODate);
        post.lastCheckedTime = QDateTime::fromString(obj["lastCheckedTime"].toString(), Qt::ISODate);
        return post;
    }
};

#endif // POST_H
