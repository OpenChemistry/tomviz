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
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace tomviz {

PassiveAcquisitionWidget::PassiveAcquisitionWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::PassiveAcquisitionWidget),
    m_client(new AcquisitionClient("http://localhost:8080/acquisition", this)),
    m_connectParamsWidget(new QWidget), m_watchTimer(new QTimer)
{
  m_ui->setupUi(this);
  this->setWindowFlags(Qt::Dialog);

  //  connect(m_ui->connectButton, SIGNAL(clicked(bool)),
  //  SLOT(connectToServer()));
  //  connect(m_ui->disconnectButton, SIGNAL(clicked(bool)),
  //          SLOT(disconnectFromServer()));
  //  connect(m_ui->previewButton, SIGNAL(clicked(bool)), SLOT(setTiltAngle()));
  //  connect(m_ui->introspectButton, &QPushButton::clicked, [this]() {
  //    this->introspectSource();
  //    m_ui->introspectButton->setEnabled(false);
  //    m_ui->connectButton->setEnabled(true);
  //  });
  //  connect(this, &AcquisitionWidget::connectParameterDescription, this,
  //          &AcquisitionWidget::generateConnectUI);
  //
  //  connect(m_ui->watchButton, &QPushButton::clicked, [this]() {
  //    if (!this->m_watchTimer->isActive()) {
  //      this->watchSource();
  //      m_ui->watchButton->setText("Stop watching");
  //    } else {
  //      this->m_watchTimer->stop();
  //      m_ui->watchButton->setText("Watch");
  //    }
  //
  //  });
  //
  //  m_ui->imageWidget->GetRenderWindow()->AddRenderer(m_renderer.Get());
  //  m_ui->imageWidget->GetInteractor()->SetInteractorStyle(
  //    m_defaultInteractorStyle.Get());
  //  m_defaultInteractorStyle->SetRenderOnMouseMove(true);
  //
  //  m_renderer->SetBackground(1.0, 1.0, 1.0);
  //  m_renderer->SetViewport(0.0, 0.0, 1.0, 1.0);

  readSettings();
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
  if (!settings->contains("acquisition/geometry")) {
    return;
  }
  //  settings->beginGroup("acquisition");
  //  setGeometry(settings->value("geometry").toRect());
  //  m_ui->splitter->restoreState(settings->value("splitterSizes").toByteArray());
  //  m_ui->hostnameEdit->setText(
  //    settings->value("hostname", "localhost").toString());
  //  m_ui->portEdit->setText(settings->value("port", "8080").toString());
  //  settings->endGroup();
}

QVariantMap PassiveAcquisitionWidget::settings()
{
  QVariantMap settingsMap;
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");
  foreach (QString key, settings->childKeys()) {
    settingsMap[key] = settings->value(key);
  }
  settings->endGroup();

  return settingsMap;
}

void PassiveAcquisitionWidget::writeSettings()
{
  //  auto settings = pqApplicationCore::instance()->settings();
  //  settings->beginGroup("acquisition");
  //  settings->setValue("geometry", geometry());
  //  settings->setValue("splitterSizes", m_ui->splitter->saveState());
  //  settings->setValue("hostname", m_ui->hostnameEdit->text());
  //  settings->setValue("port", m_ui->portEdit->text());
  //  auto connectValues =
  //    InterfaceBuilder::parameterValues(m_connectParamsWidget.data());
  //  for (QVariantMap::const_iterator iter = connectValues.begin();
  //       iter != connectValues.end(); ++iter) {
  //    settings->setValue(iter.key(), iter.value());
  //  }
  //  settings->endGroup();
}

void PassiveAcquisitionWidget::connectToServer()
{
  //  m_ui->statusEdit->setText("Attempting to connect to server...");
  //  m_client->setUrl(this->url());
  //  qDebug() << this->connectParams();
  //  auto request = m_client->connect(this->connectParams());
  //  connect(request, SIGNAL(finished(QJsonValue)), SLOT(onConnect()));
  //  connect(request, &AcquisitionClientRequest::error, this,
  //          &PassiveAcquisitionWidget::onError);
}

void PassiveAcquisitionWidget::onConnect()
{
  //  m_ui->statusEdit->setText("Connected to " + m_client->url() + "!!!");
  //  m_ui->connectButton->setEnabled(false);
  //  m_ui->disconnectButton->setEnabled(true);
  //  m_ui->watchButton->setEnabled(true);
  //  setAcquireParameters();
}

void PassiveAcquisitionWidget::disconnectFromServer()
{
  //  m_ui->statusEdit->setText("Disconnecting");
  //  auto request = m_client->disconnect(QJsonObject());
  //  connect(request, SIGNAL(finished(QJsonValue)), SLOT(onDisconnect()));
  //  connect(request, &AcquisitionClientRequest::error, this,
  //          &PassiveAcquisitionWidget::onError);
}

void PassiveAcquisitionWidget::onDisconnect()
{
  //  m_ui->statusEdit->setText("Disconnected");
  //  m_ui->connectButton->setEnabled(true);
  //  m_ui->disconnectButton->setEnabled(false);
}

void PassiveAcquisitionWidget::setAcquireParameters()
{
  //  QJsonObject params;
  //  auto request = m_client->acquisition_params(params);
  //  connect(request, SIGNAL(finished(QJsonValue)),
  //          SLOT(acquireParameterResponse(QJsonValue)));
  //  connect(request, &AcquisitionClientRequest::error, this,
  //          &PassiveAcquisitionWidget::onError);
}

void PassiveAcquisitionWidget::acquireParameterResponse(
  const QJsonValue& result)
{
  //  if (result.isObject()) {
  //    auto resultObject = result.toObject();
  //    if (resultObject.contains("units") && resultObject["units"].isString())
  //    {
  //      m_units = resultObject["units"].toString();
  //    }
  //    // Careful, at least in the impllementation examinend, X/Y are in
  //    metres.
  //    if (resultObject.contains("calX") && resultObject["calX"].isDouble()) {
  //      m_calX = resultObject["calX"].toDouble();
  //    }
  //    if (resultObject.contains("calY") && resultObject["calY"].isDouble()) {
  //      m_calX = resultObject["calY"].toDouble();
  //    }
  //    m_ui->statusEdit->setText("Pixel dimensionality received");
  //    QString pixelSize = QString::number(m_calX * 1e9) + " x " +
  //                        QString::number(m_calY * 1e9) + " nm";
  //    m_ui->pixelSize->setText(pixelSize);
  //  }
}

void PassiveAcquisitionWidget::setTiltAngle()
{
  //  QJsonObject params;
  //  params["angle"] = m_ui->tiltAngleSpinBox->value();
  //  auto request = m_client->tilt_params(params);
  //  connect(request, SIGNAL(finished(QJsonValue)),
  //          SLOT(acquirePreview(QJsonValue)));
  //  connect(request, &AcquisitionClientRequest::error, this,
  //          &AcquisitionWidget::onError);
  //
  //  m_ui->previewButton->setEnabled(false);
  //  m_ui->acquireButton->setEnabled(false);
}

void PassiveAcquisitionWidget::acquirePreview(const QJsonValue& result)
{
  //  // This should be the actual angle the stage is at.
  //  if (result.isDouble()) {
  //    m_tiltAngle = result.toDouble(-69.99);
  //    m_ui->tiltAngle->setText(QString::number(result.toDouble(-69.99), 'g',
  //    2));
  //  }
  //
  //  auto request = m_client->preview_scan();
  //  connect(request, SIGNAL(finished(QString, QByteArray)),
  //          SLOT(previewReady(QString, QByteArray)));
  //  connect(request, &AcquisitionClientRequest::error, this,
  //          &AcquisitionWidget::onError);
}

void PassiveAcquisitionWidget::previewReady(QString mimeType, QByteArray result)
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

  if (m_tiltAngle > 0.0) {
    path.append('+');
  }
  path.append(QString::number(m_tiltAngle, 'g', 2));
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

void PassiveAcquisitionWidget::resetCamera()
{
  vtkCamera* camera = m_renderer->GetActiveCamera();
  double* bounds = m_imageData->GetBounds();
  vtkVector3d point;
  point[0] = 0.5 * (bounds[0] + bounds[1]);
  point[1] = 0.5 * (bounds[2] + bounds[3]);
  point[2] = 0.5 * (bounds[4] + bounds[5]);
  camera->SetFocalPoint(point.GetData());
  point[2] += 50 + 0.5 * (bounds[4] + bounds[5]);
  camera->SetPosition(point.GetData());
  camera->SetViewUp(0.0, 1.0, 0.0);
  camera->ParallelProjectionOn();
  double parallelScale;
  if (bounds[1] - bounds[0] < bounds[3] - bounds[2]) {
    parallelScale = 0.5 * (bounds[3] - bounds[2] + 1);
  } else {
    parallelScale = 0.5 * (bounds[1] - bounds[0] + 1);
  }
  camera->SetParallelScale(parallelScale);
  double clippingRange[2];
  camera->GetClippingRange(clippingRange);
  clippingRange[1] = clippingRange[0] + (bounds[5] - bounds[4] + 50);
  camera->SetClippingRange(clippingRange);
}

void PassiveAcquisitionWidget::onError(const QString& errorMessage,
                                       const QJsonValue& errorData)
{
  qDebug() << errorMessage;
  qDebug() << errorData;
}

QString PassiveAcquisitionWidget::url() const
{
  return "";
  // return "http://" + m_ui->hostnameEdit->text() + ":" +
  // m_ui->portEdit->text() +
  //       "/acquisition";
}

void PassiveAcquisitionWidget::generateConnectUI(QJsonValue params)
{
  if (params.isArray()) {
    InterfaceBuilder* ib = new InterfaceBuilder(this);
    auto parameters = params.toArray();
    auto connectParamsLayout = new QGridLayout;
    ib->setParameterValues(this->settings());
    ib->buildParameterInterface(connectParamsLayout, parameters);
    m_connectParamsWidget->setLayout(connectParamsLayout);
    ib->deleteLater();
    // m_ui->paramsLayout->addWidget(m_connectParamsWidget);
  }
}

void PassiveAcquisitionWidget::introspectSource()
{
  m_client->setUrl(this->url());
  auto request = m_client->describe("connect");
  connect(request, &AcquisitionClientRequest::error, this,
          &PassiveAcquisitionWidget::onError);
  connect(request, &AcquisitionClientRequest::finished, this,
          &PassiveAcquisitionWidget::connectParameterDescription);
}

QJsonObject PassiveAcquisitionWidget::connectParams()
{
  auto values = InterfaceBuilder::parameterValues(this->m_connectParamsWidget);
  return QJsonObject::fromVariantMap(values);
}

void PassiveAcquisitionWidget::watchSource()
{
  connect(this->m_watchTimer, &QTimer::timeout, this,
          [this]() {
            auto request = m_client->stem_acquire();
            connect(request, &AcquisitionClientImageRequest::finished,
                    [this](const QString mimeType, const QByteArray& result,
                           const QJsonObject& meta) {
                      if (!result.isNull()) {
                        qDebug() << "New image received!";
                        this->previewReady(mimeType, result);
                      }
                    });
            connect(request, &AcquisitionClientRequest::error, this,
                    &PassiveAcquisitionWidget::onError);

          },
          Qt::UniqueConnection);
  this->m_watchTimer->start(1000);
}
}
