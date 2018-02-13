/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#ifndef ACQUISITION_CLIENT_H
#define ACQUISITION_CLIENT_H

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>

namespace tomviz {

class JsonRpcClient;
class JsonRpcReply;

class AcquisitionClientBaseRequest : public QObject
{
  Q_OBJECT

public:
  explicit AcquisitionClientBaseRequest(QObject* parent = 0) : QObject(parent)
  {
  }

signals:
  void error(const QString& errorMessage, const QJsonValue& errorData);
};

class AcquisitionClientRequest : public AcquisitionClientBaseRequest
{
  Q_OBJECT

public:
  explicit AcquisitionClientRequest(QObject* parent = 0)
    : AcquisitionClientBaseRequest(parent)
  {
  }

signals:
  void finished(const QJsonValue& result);
};

class AcquisitionClientImageRequest : public AcquisitionClientBaseRequest
{
  Q_OBJECT

public:
  explicit AcquisitionClientImageRequest(QObject* parent = 0)
    : AcquisitionClientBaseRequest(parent)
  {
  }

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
}

#endif
