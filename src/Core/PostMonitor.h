#ifndef POSTMONITOR_H
#define POSTMONITOR_H

#include <QObject>
#include <QString>
#include <QList>
#include "Data/Keyword.h"

class PostMonitor : public QObject {
    Q_OBJECT

public:
    explicit PostMonitor(QObject* parent = nullptr);

    // 获取监控脚本
    QString getMonitorScript(const QList<Keyword>& keywords);

    // 获取粉丝页面监控脚本
    QString getFollowersMonitorScript();

private:
    QString buildKeywordsArray(const QList<Keyword>& keywords);
};

#endif // POSTMONITOR_H
