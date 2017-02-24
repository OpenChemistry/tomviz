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
#include <QByteArray>
#include <QCryptographicHash>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QProcessEnvironment>

#include "AcquisitionClient.h"
#include "OperatorPython.h"
#include "TomvizTest.h"

using namespace tomviz;

class AcquisitionClientTest : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {

    QString python = "python";
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains("TOMVIZ_TEST_PYTHON_EXECUTABLE")) {
      python = env.value("TOMVIZ_TEST_PYTHON_EXECUTABLE");
    }

    QStringList arguments;
    arguments << "-m"
              << "tomviz";

    server = new QProcess();
    server->start(python, arguments, QIODevice::ReadWrite);
    server->setProcessChannelMode(QProcess::MergedChannels);
    server->waitForStarted();

    // Wait for server to start ( returns a 404 for a invalid URL )
    QNetworkAccessManager* manager = new QNetworkAccessManager();
    QObject::connect(manager, &QNetworkAccessManager::finished,
                     [this, manager](QNetworkReply* networkReply) {
                       int statusCode =
                         networkReply
                           ->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                           .toInt();
                       if (statusCode == 404) {
                         this->serverStarted = true;
                       }

                     });

    int tries = 10;
    while (!this->serverStarted && tries != 0) {
      manager->get(QNetworkRequest(QUrl(this->url)));
      QCoreApplication::processEvents();
      QThread::currentThread()->sleep(1);
      tries--;
    }
  }

  void cleanupTestCase() { this->server->kill(); }

  void connectTest() { this->connect(); }

  void disconnectTest()
  {
    // Connect first
    this->connect();

    AcquisitionClient client(this->url);
    QJsonObject params;
    AcquisitionClientRequest* request = client.disconnect(params);

    QSignalSpy error(request, &AcquisitionClientRequest::error);
    QSignalSpy finished(request, &AcquisitionClientRequest::finished);
    finished.wait();

    QVERIFY(error.isEmpty());
    QCOMPARE(finished.size(), 1);
    QList<QVariant> arguments = finished.takeFirst();
    QCOMPARE(arguments.size(), 1);
    QVERIFY(!arguments.at(0).toJsonValue().toBool());
  }

  void tiltParamsTest() { this->setTiltAngle(3.0); }

  void acquisitionParamsGetTest()
  {
    AcquisitionClient client(this->url);

    QJsonObject params;

    AcquisitionClientRequest* request = client.acquisition_params(params);

    QSignalSpy error(request, &AcquisitionClientRequest::error);
    QSignalSpy finished(request, &AcquisitionClientRequest::finished);
    finished.wait();

    if (!error.isEmpty()) {
      qDebug() << error;
    }
    QVERIFY(error.isEmpty());
    QCOMPARE(finished.size(), 1);
    QList<QVariant> arguments = finished.takeFirst();
    QJsonObject expected;
    expected["foo"] = "foo";
    expected["test"] = 1;
    QCOMPARE(arguments.at(0).toJsonValue().toObject(), expected);
  }

  void acquisitionParamsSetTest()
  {
    AcquisitionClient client(this->url);

    QJsonObject params;
    params["foo"] = "bar";

    AcquisitionClientRequest* request = client.acquisition_params(params);

    QSignalSpy error(request, &AcquisitionClientRequest::error);
    QSignalSpy finished(request, &AcquisitionClientRequest::finished);
    finished.wait();

    if (!error.isEmpty()) {
      qDebug() << error;
    }
    QVERIFY(error.isEmpty());
    QCOMPARE(finished.size(), 1);
    QList<QVariant> arguments = finished.takeFirst();
    QJsonObject expected;
    expected["foo"] = "bar";
    expected["test"] = 1;
    QCOMPARE(arguments.at(0).toJsonValue().toObject(), expected);
  }

  void acquisitionPreviewScanTest()
  {
    setTiltAngle(0.0);
    AcquisitionClient client(this->url);

    AcquisitionClientImageRequest* request = client.preview_scan();
    QSignalSpy error(request, &AcquisitionClientImageRequest::error);
    QSignalSpy finished(request, &AcquisitionClientImageRequest::finished);
    finished.wait();

    if (!error.isEmpty()) {
      qDebug() << error;
    }
    QVERIFY(error.isEmpty());
    QCOMPARE(finished.size(), 1);
    QList<QVariant> arguments = finished.takeFirst();
    QCOMPARE(arguments[0].toString().toLatin1().data(), "image/tiff");
    QByteArray data = arguments[1].toByteArray();
    QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
    hash.addData(data);
    QCOMPARE(hash.result().toHex().data(), "1b9723cd7e9ecd54f28c7dae13e38511");
  }

  void stemAcquireScanTest()
  {
    setTiltAngle(0.0);
    AcquisitionClient client(this->url);
    AcquisitionClientImageRequest* request = client.stem_acquire();
    QSignalSpy error(request, &AcquisitionClientImageRequest::error);
    QSignalSpy finished(request, &AcquisitionClientImageRequest::finished);
    finished.wait();

    if (!error.isEmpty()) {
      qDebug() << error;
    }
    QVERIFY(error.isEmpty());
    QCOMPARE(finished.size(), 1);
    QList<QVariant> arguments = finished.takeFirst();
    QCOMPARE(arguments[0].toString().toLatin1().data(), "image/tiff");
    QByteArray data = arguments[1].toByteArray();
    QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
    hash.addData(data);
    QCOMPARE(hash.result().toHex().data(), "1b9723cd7e9ecd54f28c7dae13e38511");
  }

private:
  QProcess* server;
  bool serverStarted = false;
  QString url = "http://localhost:8080/acquisition/";

  void connect()
  {
    AcquisitionClient client(this->url);
    QJsonObject params;
    AcquisitionClientRequest* request = client.connect(params);

    QSignalSpy error(request, &AcquisitionClientRequest::error);
    QSignalSpy finished(request, &AcquisitionClientRequest::finished);
    finished.wait();

    QVERIFY(error.isEmpty());
    QCOMPARE(finished.size(), 1);
    QList<QVariant> arguments = finished.takeFirst();
    QCOMPARE(arguments.size(), 1);
    QVERIFY(arguments.at(0).toJsonValue().toBool());
  }

  void setTiltAngle(double angle)
  {
    AcquisitionClient client(this->url);

    QJsonObject params;
    params["angle"] = angle;
    AcquisitionClientRequest* request = client.tilt_params(params);

    QSignalSpy error(request, &AcquisitionClientRequest::error);
    QSignalSpy finished(request, &AcquisitionClientRequest::finished);
    finished.wait();

    if (!error.isEmpty()) {
      qDebug() << error;
    }
    QVERIFY(error.isEmpty());
    QCOMPARE(finished.size(), 1);
    QList<QVariant> arguments = finished.takeFirst();
    QCOMPARE(arguments.at(0).toJsonValue().toDouble(), angle);
  }
};

QTEST_GUILESS_MAIN(AcquisitionClientTest)
#include "AcquisitionClientTest.moc"
