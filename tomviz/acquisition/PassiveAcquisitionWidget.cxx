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
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

namespace tomviz {

const char* PASSIVE_ADAPTER =
  "tomviz.acquisition.vendors.passive.PassiveWatchSource";

PassiveAcquisitionWidget::PassiveAcquisitionWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::PassiveAcquisitionWidget),
    m_client(new AcquisitionClient("http://localhost:8080/acquisition", this)),
    m_connectParamsWidget(new QWidget), m_watchTimer(new QTimer)
{
  m_ui->setupUi(this);
  this->setWindowFlags(Qt::Dialog);

  // Default to home directory
  QStringList locations =
    QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
  m_ui->watchPathLineEdit->setText(locations[0]);

  // Default file name regex
  m_ui->fileNameRegexLineEdit->setText(".*_([n,p]{1}[\\d,\\.]+)degree.*\\.dm3");

  readSettings();

  connect(m_ui->watchPathLineEdit, &QLineEdit::textChanged, this,
          &PassiveAcquisitionWidget::checkEnableWatchButton);
  connect(m_ui->connectionsWidget, &ConnectionsWidget::selectionChanged, this,
          &PassiveAcquisitionWidget::checkEnableWatchButton);

  connect(m_ui->watchButton, &QPushButton::clicked, [this]() {

    // Validate the filename regex
    if (!this->validateRegex()) {
      return;
    }

    this->m_retryCount = 5;
    this->connectToServer();
  });

  connect(m_ui->stopWatchingButton, &QPushButton::clicked, this,
          &PassiveAcquisitionWidget::stopWatching);

  connect(m_ui->fileNameRegexLineEdit, &QLineEdit::textChanged, [this]() {
    this->setEnabledRegexGroupsWidget(
      !m_ui->fileNameRegexLineEdit->text().isEmpty());
  });
  this->setEnabledRegexGroupsWidget(
    !m_ui->fileNameRegexLineEdit->text().isEmpty());

  connect(m_ui->regexGroupsWidget, &RegexGroupsWidget::groupsChanged, [this]() {
    this->setEnabledRegexGroupsSubstitutionsWidget(
      !this->m_ui->regexGroupsWidget->regexGroups().isEmpty());
  });
  this->setEnabledRegexGroupsSubstitutionsWidget(
    !this->m_ui->regexGroupsWidget->regexGroups().isEmpty());

  this->checkEnableWatchButton();

  // Connect signal to clean up any servers we start.
  auto app = QCoreApplication::instance();
  connect(app, &QApplication::aboutToQuit, [this]() {
    if (this->m_serverProcess != nullptr) {
      // First disconnect the error signal as we are about pull the rug from
      // under
      // the process!
      disconnect(this->m_serverProcess, &QProcess::errorOccurred, nullptr,
                 nullptr);
      this->m_serverProcess->terminate();
    }
  });

  // Setup regex error label
  auto palette = m_regexErrorLabel.palette();
  palette.setColor(m_regexErrorLabel.foregroundRole(), Qt::red);
  m_regexErrorLabel.setPalette(palette);

  connect(m_ui->fileNameRegexLineEdit, &QLineEdit::textChanged, [this]() {
    this->m_ui->formLayout->removeWidget(&m_regexErrorLabel);
    this->m_regexErrorLabel.setText("");
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
  if (!settings->contains("acquisition/passive.geometry")) {
    return;
  }
  settings->beginGroup("acquisition");
  setGeometry(settings->value("passive.geometry").toRect());
  m_ui->splitter->restoreState(
    settings->value("passive.splitterSizes").toByteArray());
  auto watchPath = settings->value("watchPath").toString();
  if (!watchPath.isEmpty()) {
    m_ui->watchPathLineEdit->setText(watchPath);
  }

  auto fileNameRegex = settings->value("fileNameRegex").toString();
  if (!fileNameRegex.isEmpty()) {
    m_ui->fileNameRegexLineEdit->setText(fileNameRegex);
  }

  settings->endGroup();
}

void PassiveAcquisitionWidget::writeSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");
  settings->setValue("passive.geometry", geometry());
  settings->setValue("passive.splitterSizes", m_ui->splitter->saveState());
  settings->setValue("watchPath", m_ui->watchPathLineEdit->text());
  settings->setValue("fileNameRegex", m_ui->fileNameRegexLineEdit->text());
  settings->endGroup();
}

void PassiveAcquisitionWidget::connectToServer(bool startServer)
{
  if (this->m_retryCount == 0) {
    this->displayError("Retry count excceed trying to connect to server.");
    return;
  }

  m_client->setUrl(this->url());
  auto request = m_client->connect(this->connectParams());
  connect(request, &AcquisitionClientRequest::finished, [this]() {
    // Now check that we are connected to server that has the right adapter
    // loaded.
    auto describeRequest = this->m_client->describe();
    connect(describeRequest, &AcquisitionClientRequest::error, this,
            &PassiveAcquisitionWidget::onError);
    connect(
      describeRequest, &AcquisitionClientRequest::finished,
      [this](const QJsonValue& result) {
        if (!result.isObject()) {
          this->onError("Invalid response to describe request:", result);
          return;
        }

        if (result.toObject()["name"] != PASSIVE_ADAPTER) {
          this->onError(
            "The server is not running the passive acquisition "
            "adapter, please restart the server with the correct adapter.",
            QJsonValue());
          return;
        }

        // Now we can start watching.
        this->watchSource();

      });
  });

  connect(request, &AcquisitionClientRequest::error,
          [startServer, this](const QString& errorMessage,
                              const QJsonValue& errorData) {

            auto connection =
              this->m_ui->connectionsWidget->selectedConnection();
            // If we are getting a connection refused error and we are trying to
            // connect
            // to localhost, try to start the server.
            if (startServer &&
                errorData.toInt() == QNetworkReply::ConnectionRefusedError &&
                connection->hostName() == "localhost") {
              this->startLocalServer();
            } else {
              this->onError(errorMessage, errorData);
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

  this->stopWatching();
  this->displayError(message);
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
  this->m_ui->watchButton->setEnabled(false);
  this->m_ui->stopWatchingButton->setEnabled(true);
  connect(this->m_watchTimer, &QTimer::timeout, this,
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
                        this->imageReady(mimeType, result, angle);
                      }
                    });
            connect(request, &AcquisitionClientRequest::error, this,
                    &PassiveAcquisitionWidget::onError);

          },
          Qt::UniqueConnection);
  this->m_watchTimer->start(1000);
}

QJsonObject PassiveAcquisitionWidget::connectParams()
{
  QJsonObject connectParams{
    { "path", m_ui->watchPathLineEdit->text() },
    { "fileNameRegex", m_ui->fileNameRegexLineEdit->text() },
  };

  auto fileNameRegexGroups = m_ui->regexGroupsWidget->regexGroups();
  if (!fileNameRegexGroups.isEmpty()) {
    auto groups = QJsonArray::fromStringList(fileNameRegexGroups);
    connectParams["fileNameRegexGroups"] = groups;
  }

  auto regexGroupsSubstitutions =
    m_ui->regexGroupsSubstitutionsWidget->substitutions();
  if (!regexGroupsSubstitutions.isEmpty()) {
    QJsonObject substitutions;
    foreach (RegexGroupSubstitution sub, regexGroupsSubstitutions) {

      QJsonArray regexToSubs;
      if (substitutions.contains(sub.groupName())) {
        regexToSubs = substitutions.value(sub.groupName()).toArray();
      }
      QJsonObject mapping;
      mapping[sub.regex()] = sub.substitution();
      regexToSubs.append(mapping);
      substitutions[sub.groupName()] = regexToSubs;
    }
    connectParams["groupRegexSubstitutions"] = substitutions;
  }

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

  this->m_serverProcess = new QProcess(this);
  this->m_serverProcess->setProgram(pythonExecutablePath);
  this->m_serverProcess->setArguments(arguments);
  connect(this->m_serverProcess, &QProcess::errorOccurred,
          [this](QProcess::ProcessError error) {
            Q_UNUSED(error);
            auto message = QString("Error starting local acquisition: '%1'")
                             .arg(this->m_serverProcess->errorString());

            QMessageBox::warning(this, "Server Start Error", message,
                                 QMessageBox::Ok);

          });

  connect(this->m_serverProcess, &QProcess::started, [this]() {
    // Now try to connect and watch. Note we are not asking for server to be
    // started if the connection fails, this is to prevent us getting into a
    // connect loop.
    QTimer::singleShot(200, [this]() {
      this->m_retryCount--;
      this->connectToServer(false);
    });

  });

  connect(
    this->m_serverProcess,
    static_cast<void (QProcess::*)(int)>(&QProcess::finished),
    [this](int exitCode) {
      qWarning() << QString(
                      "The acquisition server has exited with exit code: %1")
                      .arg(exitCode);
    });

  connect(this->m_serverProcess, &QProcess::readyReadStandardError, [this]() {
    qWarning() << this->m_serverProcess->readAllStandardError();
  });

  connect(this->m_serverProcess, &QProcess::readyReadStandardOutput, [this]() {
    qInfo() << this->m_serverProcess->readAllStandardOutput();
  });

  qInfo() << QString("Starting server with following command: %1 %2")
               .arg(m_serverProcess->program())
               .arg(this->m_serverProcess->arguments().join(" "));
  QStringList locations =
    QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
  qInfo()
    << QString(
         "Server logs are written to the following path: %1%2.tomviz%2logs%2")
         .arg(locations[0])
         .arg(QDir::separator());

  this->m_serverProcess->start();
}

void PassiveAcquisitionWidget::checkEnableWatchButton()
{
  auto path = m_ui->watchPathLineEdit->text();
  this->m_ui->watchButton->setEnabled(
    !path.isEmpty() &&
    this->m_ui->connectionsWidget->selectedConnection() != nullptr);
}

void PassiveAcquisitionWidget::setEnabledRegexGroupsWidget(bool enabled)
{
  this->m_ui->regexGroupsLabel->setEnabled(enabled);
  this->m_ui->regexGroupsWidget->setEnabled(enabled);
}

void PassiveAcquisitionWidget::setEnabledRegexGroupsSubstitutionsWidget(
  bool enabled)
{
  this->m_ui->regexGroupsSubstitutionsLabel->setEnabled(enabled);
  this->m_ui->regexGroupsSubstitutionsWidget->setEnabled(enabled);
}

void PassiveAcquisitionWidget::stopWatching()
{
  this->m_watchTimer->stop();
  this->m_ui->stopWatchingButton->setEnabled(false);
  this->m_ui->watchButton->setEnabled(true);
}

bool PassiveAcquisitionWidget::validateRegex()
{
  auto regExText = m_ui->fileNameRegexLineEdit->text();
  if (!regExText.isEmpty()) {
    QRegExp regExp(regExText);
    if (!regExp.isValid()) {
      m_regexErrorLabel.setText(regExp.errorString());
      m_ui->formLayout->insertRow(3, "", &m_regexErrorLabel);
      return false;
    }
  }

  return true;
}
}
