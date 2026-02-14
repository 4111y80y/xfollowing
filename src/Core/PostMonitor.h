#ifndef POSTMONITOR_H
#define POSTMONITOR_H

#include "Data/Keyword.h"
#include <QList>
#include <QObject>
#include <QString>

class PostMonitor : public QObject {
  Q_OBJECT

public:
  explicit PostMonitor(QObject *parent = nullptr);

  // 获取监控脚本
  QString getMonitorScript(const QList<Keyword> &keywords);

  // 获取粉丝页面监控脚本
  QString getFollowersMonitorScript();

  // 获取回关探测脚本（用于verified_followers页面）
  QString getFollowBackDetectScript();

private:
  QString buildKeywordsArray(const QList<Keyword> &keywords);
};

#endif // POSTMONITOR_H
