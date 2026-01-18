#ifndef AUTOFOLLOWER_H
#define AUTOFOLLOWER_H

#include <QObject>
#include <QString>

class AutoFollower : public QObject {
    Q_OBJECT

public:
    explicit AutoFollower(QObject* parent = nullptr);

    // 获取关注脚本
    QString getFollowScript();
};

#endif // AUTOFOLLOWER_H
