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

#include "AcquisitionClient.h"

#include "JsonRpcClient.h"
#include <QNetworkAccessManager>

namespace tomviz {

AcquisitionClient::AcquisitionClient(const QString& url, QObject* parent)
  : QObject(parent), m_jsonRpcClient(new JsonRpcClient(url, this))
{
}

AcquisitionClient::~AcquisitionClient() = default;

void AcquisitionClient::setUrl(const QString& url)
{
  m_jsonRpcClient->setUrl(url);
}

QString AcquisitionClient::url() const
{
  return m_jsonRpcClient->url();
}

AcquisitionClientRequest* AcquisitionClient::connect(const QJsonObject& params)
{
  return makeRequest("connect", params);
}

AcquisitionClientRequest* AcquisitionClient::disconnect(
  const QJsonObject& params)
{
  return makeRequest("disconnect", params);
}

AcquisitionClientRequest* AcquisitionClient::tilt_params(
  const QJsonObject& params)
{
  return makeRequest("tilt_params", params);
}

AcquisitionClientImageRequest* AcquisitionClient::preview_scan()
{
  return makeImageRequest("preview_scan");
}

AcquisitionClientRequest* AcquisitionClient::acquisition_params(
  const QJsonObject& params)
{
  return makeRequest("acquisition_params", params);
}

AcquisitionClientImageRequest* AcquisitionClient::stem_acquire()
{
  return makeImageRequest("stem_acquire");
}

AcquisitionClientRequest* AcquisitionClient::describe(const QString& method)
{
  QJsonObject params;
  params["method"] = method;

  return makeRequest("describe", params);
}

AcquisitionClientRequest* AcquisitionClient::describe()
{
  QJsonObject params;

  return makeRequest("describe", params);
}

AcquisitionClientRequest* AcquisitionClient::makeRequest(
  const QString& method, const QJsonObject& params)
{
  QJsonObject jsonRequest;
  jsonRequest["method"] = method;
  jsonRequest["params"] = params;

  JsonRpcReply* reply = m_jsonRpcClient->sendRequest(jsonRequest);
  AcquisitionClientRequest* request = new AcquisitionClientRequest(this);
  connectErrorSignals(reply, request);
  connectResultSignal(reply, request);

  return request;
}

AcquisitionClientImageRequest* AcquisitionClient::makeImageRequest(
  const QString& method)
{
  QJsonObject jsonRequest;
  jsonRequest["method"] = method;

  JsonRpcReply* reply = m_jsonRpcClient->sendRequest(jsonRequest);
  AcquisitionClientImageRequest* request =
    new AcquisitionClientImageRequest(this);
  connectErrorSignals(reply, request);
  connectResultSignal(reply, request);

  return request;
}

void AcquisitionClient::connectResultSignal(JsonRpcReply* reply,
                                            AcquisitionClientRequest* request)
{

  QObject::connect(reply, &JsonRpcReply::resultReceived,
                   [reply, request](QJsonObject message) {
                     QJsonValue result = message["result"];
                     emit request->finished(result);
                     reply->deleteLater();
                   });
}

void AcquisitionClient::connectResultSignal(
  JsonRpcReply* reply, AcquisitionClientImageRequest* request)
{

  QObject::connect(
    reply, &JsonRpcReply::resultReceived,
    [this, reply, request](QJsonObject message) {
      QJsonValue result = message["result"];

      auto urlMissing = [request, reply, result]() {
        request->error("Response doesn't contain URL.", result);
        reply->deleteLater();
      };
      QString url;
      QJsonObject meta;
      if (result.isString()) {
        url = result.toString();
      } else if (result.isObject()) {
        auto obj = result.toObject();
        if (!obj.contains("imageUrl")) {
          urlMissing();
          return;
        }
        url = obj["imageUrl"].toString();
        if (obj.contains("meta")) {
          meta = obj["meta"].toObject();
        }
      } else if (result.isNull()) {
        QByteArray empty;
        QJsonObject noMeta;
        emit request->finished("", empty, noMeta);
        return;
      } else {
        urlMissing();
        return;
      }

      QNetworkAccessManager* manager = new QNetworkAccessManager(this);
      QObject::connect(
        manager, &QNetworkAccessManager::finished,
        [manager, request, meta](QNetworkReply* networkReply) {
          if (networkReply->error() != QNetworkReply::NoError) {
            QJsonValue data(networkReply->error());
            emit request->error(networkReply->errorString(), data);
          } else {
            QString mimeType =
              networkReply->header(QNetworkRequest::ContentTypeHeader)
                .toString();
            QByteArray imageData = networkReply->readAll();
            emit request->finished(mimeType, imageData, meta);
          }
        });

      manager->get(QNetworkRequest(QUrl(url)));
    });
}

void AcquisitionClient::connectErrorSignals(
  JsonRpcReply* reply, AcquisitionClientBaseRequest* request)
{

  QObject::connect(reply, &JsonRpcReply::errorReceived,
                   [reply, request](QJsonObject error) {
                     QString message = error["message"].toString();
                     QJsonValue data = error["data"];
                     emit request->error(message, data);
                     reply->deleteLater();
                   });

  QObject::connect(reply, &JsonRpcReply::protocolError,
                   [reply, request](const QString& errorMessage) {
                     QJsonValue empty;
                     emit request->error(errorMessage, empty);
                     reply->deleteLater();
                   });

  QObject::connect(reply, &JsonRpcReply::parseError,
                   [reply, request](QJsonParseError::ParseError code,
                                    const QString& errorMessage) {
                     QJsonValue data(code);
                     emit request->error(errorMessage, data);
                     reply->deleteLater();
                   });

  QObject::connect(reply, &JsonRpcReply::networkError,
                   [reply, request](QNetworkReply::NetworkError code,
                                    const QString& errorMessage) {
                     QJsonValue data(code);
                     emit request->error(errorMessage, data);
                     reply->deleteLater();
                   });

  QObject::connect(
    reply, &JsonRpcReply::httpError,
    [reply, request](int statusCode, const QString& errorMessage) {
      QJsonValue data(statusCode);
      emit request->error(errorMessage, data);
      reply->deleteLater();
    });
}
}
