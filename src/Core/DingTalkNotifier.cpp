#include "DingTalkNotifier.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

DingTalkNotifier::DingTalkNotifier(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_webhook("https://oapi.dingtalk.com/robot/"
                "send?access_token="
                "0c41cc100f36dd995684453debf4e8690f34efa457692da609a4d663"
                "c866d67d"),
      m_secret("SECae54fda1927685938b81cd56c6949e3fa02b1180e4cfabc99827c7"
               "9c106d6485") {}

QString DingTalkNotifier::buildSignedUrl() {
  if (m_secret.isEmpty())
    return m_webhook;

  qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
  QString stringToSign = QString("%1\n%2").arg(timestamp).arg(m_secret);

  QByteArray hmacResult = QMessageAuthenticationCode::hash(
      stringToSign.toUtf8(), m_secret.toUtf8(), QCryptographicHash::Sha256);

  QString sign = QUrl::toPercentEncoding(hmacResult.toBase64());

  QString sep = m_webhook.contains("?") ? "&" : "?";
  return QString("%1%2timestamp=%3&sign=%4")
      .arg(m_webhook, sep, QString::number(timestamp), sign);
}

void DingTalkNotifier::sendText(const QString &content) {
  QString url = buildSignedUrl();

  QJsonObject textObj;
  textObj["content"] = content;

  QJsonObject payload;
  payload["msgtype"] = "text";
  payload["text"] = textObj;

  QNetworkRequest request(QUrl(url));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QNetworkReply *reply = m_networkManager->post(
      request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      int errcode = doc.object()["errcode"].toInt(-1);
      if (errcode == 0) {
        qDebug() << "[DingTalk] Send success";
        emit sendSuccess();
      } else {
        QString errmsg = doc.object()["errmsg"].toString();
        qDebug() << "[DingTalk] API error:" << errcode << errmsg;
        emit sendFailed(errmsg);
      }
    } else {
      qDebug() << "[DingTalk] Network error:" << reply->errorString();
      emit sendFailed(reply->errorString());
    }
  });
}
