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

#include "PassiveAcquisitionWidget.h"

#include "ui_PassiveAcquisitionWidget.h"

#include "AcquisitionClient.h"
#include "ActiveObjects.h"
#include "ConnectionDialog.h"
#include "InterfaceBuilder.h"
#include "StartServerDialog.h"

#include "DataSource.h"
#include "ModuleManager.h"
#include "Pipeline.h"
#include "PipelineManager.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <vtkSMProxy.h>

#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkTIFFReader.h>

#include <QBuffer>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonValueRef>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPalette>
#include <QProcess>
#include <QPushButton>
#include <QRegExp>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace tomviz {

const char* PASSIVE_ADAPTER =
  "tomviz.acquisition.vendors.passive.PassiveWatchSource";

PassiveAcquisitionWidget::PassiveAcquisitionWidget(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::PassiveAcquisitionWidget),
    m_client(new AcquisitionClient("http://localhost:8080/acquisition", this)),
    m_connectParamsWidget(new QWidget), m_watchTimer(new QTimer)
{
  m_ui->setupUi(this);
  // setFixedSize(QSize(420, 440));

  // Default to home directory
  QStringList locations =
    QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
  m_ui->watchPathLineEdit->setText(locations[0]);

  setupTestTable();

  readSettings();

  connect(m_ui->watchPathLineEdit, &QLineEdit::textChanged, this,
          &PassiveAcquisitionWidget::checkEnableWatchButton);
  connect(m_ui->connectionsWidget, &ConnectionsWidget::selectionChanged, this,
          &PassiveAcquisitionWidget::checkEnableWatchButton);

  connect(m_ui->formatTabWidget, &QTabWidget::currentChanged, this,
          &PassiveAcquisitionWidget::formatTabChanged);

  connect(m_ui->testFileFormatEdit, &QLineEdit::textChanged, this,
          &PassiveAcquisitionWidget::testFileNameChanged);

  connect(m_ui->basicTab, &BasicFormatWidget::regexChanged, this,
          &PassiveAcquisitionWidget::onRegexChanged);

  connect(m_ui->watchButton, &QPushButton::clicked, [this]() {
    m_retryCount = 5;
    connectToServer();
  });

  connect(m_ui->stopWatchingButton, &QPushButton::clicked, this,
          &PassiveAcquisitionWidget::stopWatching);

  checkEnableWatchButton();

  // Connect signal to clean up any servers we start.
  auto app = QCoreApplication::instance();
  connect(app, &QApplication::aboutToQuit, [this]() {
    if (m_serverProcess != nullptr) {
      // First disconnect the error signal as we are about pull the rug from
      // under
      // the process!
      disconnect(m_serverProcess, &QProcess::errorOccurred, nullptr, nullptr);
      m_serverProcess->terminate();
    }
  });

}

PassiveAcquisitionWidget::~PassiveAcquisitionWidget() = default;

void PassiveAcquisitionWidget::closeEvent(QCloseEvent* event)
{
  writeSettings();
  event->accept();
}

void PassiveAcquisitionWidget::readSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  if (!settings->contains("acquisition/watchPath")) {
    return;
  }
  settings->beginGroup("acquisition");
  setGeometry(settings->value("passive.geometry").toRect());
  
  auto watchPath = settings->value("watchPath").toString();
  if (!watchPath.isEmpty()) {
    m_ui->watchPathLineEdit->setText(watchPath);
  }

  settings->endGroup();
}

void PassiveAcquisitionWidget::writeSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");
  settings->setValue("passive.geometry", geometry());
  settings->setValue("watchPath", m_ui->watchPathLineEdit->text());
  settings->endGroup();
}

void PassiveAcquisitionWidget::connectToServer(bool startServer)
{
  if (m_retryCount == 0) {
    displayError("Retry count excceed trying to connect to server.");
    return;
  }

  m_client->setUrl(url());
  auto request = m_client->connect(connectParams());
  connect(request, &AcquisitionClientRequest::finished, [this]() {
    // Now check that we are connected to server that has the right adapter
    // loaded.
    auto describeRequest = m_client->describe();
    connect(describeRequest, &AcquisitionClientRequest::error, this,
            &PassiveAcquisitionWidget::onError);
    connect(
      describeRequest, &AcquisitionClientRequest::finished,
      [this](const QJsonValue& result) {
        if (!result.isObject()) {
          onError("Invalid response to describe request:", result);
          return;
        }

        if (result.toObject()["name"] != PASSIVE_ADAPTER) {
          onError(
            "The server is not running the passive acquisition "
            "adapter, please restart the server with the correct adapter.",
            QJsonValue());
          return;
        }

        // Now we can start watching.
        watchSource();
      });
  });

  connect(request, &AcquisitionClientRequest::error,
          [startServer, this](const QString& errorMessage,
                              const QJsonValue& errorData) {
            auto connection = m_ui->connectionsWidget->selectedConnection();
            // If we are getting a connection refused error and we are trying to
            // connect
            // to localhost, try to start the server.
            if (startServer &&
                errorData.toInt() == QNetworkReply::ConnectionRefusedError &&
                connection->hostName() == "localhost") {
              startLocalServer();
            } else {
              onError(errorMessage, errorData);
            }
          });
}

void PassiveAcquisitionWidget::imageReady(QString mimeType, QByteArray result,
                                          float angle)
{
  if (mimeType != "image/tiff") {
    qDebug() << "image/tiff is the only supported mime type right now.\n"
             << mimeType << "\n";
    return;
  }

  QDir dir(QDir::homePath() + "/tomviz-data");
  if (!dir.exists()) {
    dir.mkpath(dir.path());
  }

  QString path = "/tomviz_";

  if (angle > 0.0) {
    path.append('+');
  }
  path.append(QString::number(angle, 'g', 2));
  path.append(".tiff");

  QFile file(dir.path() + path);
  file.open(QIODevice::WriteOnly);
  file.write(result);
  qDebug() << "Data file:" << file.fileName();
  file.close();

  vtkNew<vtkTIFFReader> reader;
  reader->SetFileName(file.fileName().toLatin1());
  reader->Update();
  m_imageData = reader->GetOutput();
  m_imageSlice->GetProperty()->SetInterpolationTypeToNearest();
  m_imageSliceMapper->SetInputData(m_imageData.Get());
  m_imageSliceMapper->Update();
  m_imageSlice->SetMapper(m_imageSliceMapper.Get());
  m_renderer->AddViewProp(m_imageSlice.Get());

  // If we haven't added it, add our live data source to the pipeline.
  if (!m_dataSource) {
    m_dataSource = new DataSource(m_imageData);
    m_dataSource->setLabel("Live!");
    auto pipeline = new Pipeline(m_dataSource);
    PipelineManager::instance().addPipeline(pipeline);
    ModuleManager::instance().addDataSource(m_dataSource);
    pipeline->addDefaultModules(m_dataSource);
  } else {
    m_dataSource->appendSlice(m_imageData);
  }
}

void PassiveAcquisitionWidget::onError(const QString& errorMessage,
                                       const QJsonValue& errorData)
{
  auto message = errorMessage;
  if (!errorData.toString().isEmpty()) {
    message = QString("%1\n%2").arg(message).arg(errorData.toString());
  }

  stopWatching();
  displayError(message);
}

void PassiveAcquisitionWidget::displayError(const QString& errorMessage)
{
  QMessageBox::warning(this, "Acquisition Error", errorMessage,
                       QMessageBox::Ok);
}

QString PassiveAcquisitionWidget::url() const
{
  auto connection = m_ui->connectionsWidget->selectedConnection();

  return QString("http://%1:%2/acquisition")
    .arg(connection->hostName())
    .arg(connection->port());
}

void PassiveAcquisitionWidget::watchSource()
{
  m_ui->watchButton->setEnabled(false);
  m_ui->stopWatchingButton->setEnabled(true);
  connect(m_watchTimer, &QTimer::timeout, this,
          [this]() {
            auto request = m_client->stem_acquire();
            connect(request, &AcquisitionClientImageRequest::finished,
                    [this](const QString mimeType, const QByteArray& result,
                           const QJsonObject& meta) {
                      if (!result.isNull()) {
                        float angle = 0;
                        if (meta.contains("angle")) {
                          angle = meta["angle"].toString().toFloat();
                        }
                        imageReady(mimeType, result, angle);
                      }
                    });
            connect(request, &AcquisitionClientRequest::error, this,
                    &PassiveAcquisitionWidget::onError);
          },
          Qt::UniqueConnection);
  m_watchTimer->start(1000);
}

QJsonObject PassiveAcquisitionWidget::connectParams()
{
  /*
    Let's write some json in C++ !!
    
    Structure:

    connectParams = {
      "path": "/directory/to/be/watched/",
      "fileNameRegex": "^.*((n|p)?(\d+(\.\d+)?)).*(\.tif[f]?)$",
      "fileNameRegexGroups": [
        "angle"
      ],
      "groupRegexSubstitutions": {
        "angle": [
          {"n": "-"},
          {"p": "+"},
        ]
      }
    }
  */

  QJsonObject connectParams{
    { "path", m_ui->watchPathLineEdit->text() },
  };

  QString regex;
  QJsonArray groups;
  QJsonObject substitutions;

  int tabIndex = m_ui->formatTabWidget->currentIndex();
  if (tabIndex == 0) {
    regex = m_ui->basicTab->getPythonRegex();
    groups = m_ui->basicTab->getRegexGroups();
  } else {

  }

  connectParams["fileNameRegex"] = regex;
  connectParams["fileNameRegexGroups"] = groups;
  connectParams["groupRegexSubstitutions"] = substitutions;
  
  qDebug() << connectParams;
  return connectParams;
}

void PassiveAcquisitionWidget::startLocalServer()
{
  StartServerDialog dialog;
  auto r = dialog.exec();
  if (r != QDialog::Accepted) {
    return;
  }
  auto pythonExecutablePath = dialog.pythonExecutablePath();

  QStringList arguments;
  arguments << "-m"
            << "tomviz"
            << "-a" << PASSIVE_ADAPTER << "-r";

  m_serverProcess = new QProcess(this);
  m_serverProcess->setProgram(pythonExecutablePath);
  m_serverProcess->setArguments(arguments);
  connect(m_serverProcess, &QProcess::errorOccurred,
          [this](QProcess::ProcessError error) {
            Q_UNUSED(error);
            auto message = QString("Error starting local acquisition: '%1'")
                             .arg(m_serverProcess->errorString());

            QMessageBox::warning(this, "Server Start Error", message,
                                 QMessageBox::Ok);
          });

  connect(m_serverProcess, &QProcess::started, [this]() {
    // Now try to connect and watch. Note we are not asking for server to be
    // started if the connection fails, this is to prevent us getting into a
    // connect loop.
    QTimer::singleShot(200, [this]() {
      --m_retryCount;
      connectToServer(false);
    });
  });

  connect(
    m_serverProcess, static_cast<void (QProcess::*)(int)>(&QProcess::finished),
    [](int exitCode) {
      qWarning() << QString(
                      "The acquisition server has exited with exit code: %1")
                      .arg(exitCode);
    });

  connect(m_serverProcess, &QProcess::readyReadStandardError,
          [this]() { qWarning() << m_serverProcess->readAllStandardError(); });

  connect(m_serverProcess, &QProcess::readyReadStandardOutput,
          [this]() { qInfo() << m_serverProcess->readAllStandardOutput(); });

  qInfo() << QString("Starting server with following command: %1 %2")
               .arg(m_serverProcess->program())
               .arg(m_serverProcess->arguments().join(" "));
  QStringList locations =
    QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
  qInfo()
    << QString(
         "Server logs are written to the following path: %1%2.tomviz%2logs%2")
         .arg(locations[0])
         .arg(QDir::separator());

  m_serverProcess->start();
}

void PassiveAcquisitionWidget::checkEnableWatchButton()
{
  auto path = m_ui->watchPathLineEdit->text();
  m_ui->watchButton->setEnabled(!path.isEmpty() &&
                                m_ui->connectionsWidget->selectedConnection() !=
                                  nullptr);
}

void PassiveAcquisitionWidget::stopWatching()
{
  m_watchTimer->stop();
  m_ui->stopWatchingButton->setEnabled(false);
  m_ui->watchButton->setEnabled(true);
}

void PassiveAcquisitionWidget::formatTabChanged(int index)
{
  qDebug() << index;
  // Hide/Show tab index so that the tab itself is resized
  // if (index == 0) {
  //   m_ui->advancedTab->setVisible(false);
  //   m_ui->basicTab->setVisible(true);
  // } else if (index == 1) {
  //   m_ui->advancedTab->setVisible(true);
  //   m_ui->basicTab->setVisible(false);
  // }

}

void PassiveAcquisitionWidget::onRegexChanged(QString regex)
{
  qDebug() << regex;
  validateTestFileName();
}

void PassiveAcquisitionWidget::validateTestFileName()
{
  int tabIndex = m_ui->formatTabWidget->currentIndex();
  MatchInfo result;

  if (tabIndex == 0) {
    result = m_ui->basicTab->matchFileName(m_testFileName);
  } else {
   
  }
  
  if (result.matched) {
    m_ui->testFileFormatEdit->setStyleSheet("background-color : #A5D6A7;");
  } else {
    m_ui->testFileFormatEdit->setStyleSheet("background-color : #FFAB91;");
  }
  if (m_testFileName.isEmpty()) {
    m_ui->testFileFormatEdit->setStyleSheet("");
  }

  QStringList tableHeaders;
  m_ui->testTableWidget->setColumnCount(result.groups.size());
  for (int i = 0; i < result.groups.size(); ++i) {
    tableHeaders << result.groups[i].name;
    m_ui->testTableWidget->setItem(0, i, new QTableWidgetItem(result.groups[i].capturedText));
  }
  m_ui->testTableWidget->setHorizontalHeaderLabels(tableHeaders);
  resizeTestTable();
}

void PassiveAcquisitionWidget::testFileNameChanged(QString fileName)
{
  qDebug() << fileName;
  m_testFileName = fileName;
  validateTestFileName();
}

void PassiveAcquisitionWidget::setupTestTable()
{
  m_ui->testTableWidget->setRowCount(1);
  m_ui->testTableWidget->setColumnCount(1);
  QStringList tableHeaders;
  m_ui->testTableWidget->verticalHeader()->setVisible(false);
  m_ui->testTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_ui->testTableWidget->setSelectionMode(QAbstractItemView::NoSelection);

  m_ui->testTableWidget->setVisible(true);
  m_ui->testTablePlaceholder->setVisible(false);
  resizeTestTable();
  validateTestFileName();
}

void  PassiveAcquisitionWidget::resizeTestTable()
{
  m_ui->testTableWidget->resizeColumnsToContents();
  m_ui->testTableWidget->horizontalHeader()->setSectionResizeMode(
    0, QHeaderView::Stretch);
  int horizontalHeaderHeight = m_ui->testTableWidget->horizontalHeader()->height();
  int rowTotalHeight = m_ui->testTableWidget->verticalHeader()->sectionSize(0);
  int vSize = horizontalHeaderHeight+rowTotalHeight;//+scrollBarHeight;

  m_ui->testTableWidget->setMinimumHeight(vSize);
  m_ui->testTableWidget->setMaximumHeight(vSize);
  m_ui->testTablePlaceholder->setMinimumHeight(vSize);
  m_ui->testTablePlaceholder->setMaximumHeight(vSize);
}

} // namespace tomviz
