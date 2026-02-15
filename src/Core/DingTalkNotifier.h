#ifndef DINGTALKNOTIFIER_H
#define DINGTALKNOTIFIER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>


class DingTalkNotifier : public QObject {
  Q_OBJECT
public:
  explicit DingTalkNotifier(QObject *parent = nullptr);

  void sendText(const QString &content);

signals:
  void sendSuccess();
  void sendFailed(const QString &error);

private:
  QString buildSignedUrl();

  QNetworkAccessManager *m_networkManager;
  QString m_webhook;
  QString m_secret;
};

#endif // DINGTALKNOTIFIER_H
