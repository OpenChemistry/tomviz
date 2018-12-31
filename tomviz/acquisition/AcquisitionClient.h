/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef ACQUISITION_CLIENT_H
#define ACQUISITION_CLIENT_H

#include <QObject>

#include <QJsonObject>
#include <QJsonValue>

namespace tomviz {

class JsonRpcClient;
class JsonRpcReply;

class AcquisitionClientBaseRequest : public QObject
{
  Q_OBJECT

public:
  explicit AcquisitionClientBaseRequest(QObject* parent = 0) : QObject(parent)
  {}

signals:
  void error(const QString& errorMessage, const QJsonValue& errorData);
};

class AcquisitionClientRequest : public AcquisitionClientBaseRequest
{
  Q_OBJECT

public:
  explicit AcquisitionClientRequest(QObject* parent = 0)
    : AcquisitionClientBaseRequest(parent)
  {}

signals:
  void finished(const QJsonValue& result);
};

class AcquisitionClientImageRequest : public AcquisitionClientBaseRequest
{
  Q_OBJECT

public:
  explicit AcquisitionClientImageRequest(QObject* parent = 0)
    : AcquisitionClientBaseRequest(parent)
  {}

signals:
  void finished(const QString mimeType, const QByteArray& result,
                const QJsonObject& meta);
};

class AcquisitionClient : public QObject
{
  Q_OBJECT

public:
  explicit AcquisitionClient(const QString& url, QObject* parent = 0);
  ~AcquisitionClient() override;

  void setUrl(const QString& url);
  QString url() const;

public slots:

  AcquisitionClientRequest* connect(const QJsonObject& params);

  AcquisitionClientRequest* disconnect(const QJsonObject& params);

  AcquisitionClientRequest* tilt_params(const QJsonObject& params);

  AcquisitionClientImageRequest* preview_scan();

  AcquisitionClientRequest* acquisition_params(const QJsonObject& params);

  AcquisitionClientImageRequest* stem_acquire();

  AcquisitionClientRequest* describe(const QString& method);

  AcquisitionClientRequest* describe();

private slots:

  AcquisitionClientRequest* makeRequest(const QString& method,
                                        const QJsonObject& params);
  AcquisitionClientImageRequest* makeImageRequest(const QString& method);
  void connectResultSignal(JsonRpcReply* reply,
                           AcquisitionClientRequest* request);
  void connectResultSignal(JsonRpcReply* reply,
                           AcquisitionClientImageRequest* request);
  void connectErrorSignals(JsonRpcReply* reply,
                           AcquisitionClientBaseRequest* request);

private:
  JsonRpcClient* m_jsonRpcClient;
};
} // namespace tomviz

#endif
