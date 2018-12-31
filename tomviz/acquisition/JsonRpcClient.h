/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef TOMVIZ_JSONRPCCLIENT_H
#define TOMVIZ_JSONRPCCLIENT_H

#include <QObject>

#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>

class QNetworkAccessManager;

namespace tomviz {

class JsonRpcReply : public QObject
{
  Q_OBJECT

public:
  explicit JsonRpcReply(QObject* parent = 0) : QObject(parent) {}

signals:
  /// Emitted when a result is received.
  void resultReceived(QJsonObject message);

  /// Emitted when an error response is received.
  void errorReceived(QJsonObject message);

  void protocolError(const QString& errorMessage);

  void parseError(QJsonParseError::ParseError code,
                  const QString& errorMessage);

  void networkError(QNetworkReply::NetworkError code,
                    const QString& errorMessage);

  void httpError(int statusCode, const QString& errorMessage);
};

class JsonRpcClient : public QObject
{
  Q_OBJECT

public:
  explicit JsonRpcClient(const QString& url, QObject* parent = 0);

  /// Set the server URL.
  void setUrl(const QString& url) { m_url = url; }

  /// Return the server URL.
  QString url() const { return m_url; }

public slots:
  /// Send the Json request to the RPC server.
  JsonRpcReply* sendRequest(const QJsonObject& request);

signals:
  /// Emitted when a notification is received.
  void notificationReceived(QJsonObject message);

protected:
  unsigned int m_requestCounter = 0;
  QString m_url;
  QNetworkAccessManager* m_networkAccessManager = nullptr;
};
} // namespace tomviz

#endif
