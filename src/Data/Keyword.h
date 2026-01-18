#ifndef KEYWORD_H
#define KEYWORD_H

#include <QString>
#include <QJsonObject>
#include <QUuid>

struct Keyword {
    QString id;                // 关键词ID
    QString text;              // 关键词文本
    bool isEnabled = true;     // 是否启用
    int matchCount = 0;        // 匹配次数

    Keyword() {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    Keyword(const QString& keyword) : text(keyword) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["text"] = text;
        obj["isEnabled"] = isEnabled;
        obj["matchCount"] = matchCount;
        return obj;
    }

    static Keyword fromJson(const QJsonObject& obj) {
        Keyword kw;
        kw.id = obj["id"].toString();
        kw.text = obj["text"].toString();
        kw.isEnabled = obj["isEnabled"].toBool(true);
        kw.matchCount = obj["matchCount"].toInt(0);
        return kw;
    }
};

#endif // KEYWORD_H
