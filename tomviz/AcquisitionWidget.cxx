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

#include "AcquisitionWidget.h"

#include "ui_AcquisitionWidget.h"

#include "AcquisitionClient.h"
#include "ActiveObjects.h"

#include <vtkSMProxy.h>

#include <vtkCamera.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkTIFFReader.h>

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QTemporaryFile>

namespace tomviz {

AcquisitionWidget::AcquisitionWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::AcquisitionWidget),
    m_client(new AcquisitionClient("http://localhost:8080/acquisition", this))
{
  m_ui->setupUi(this);
  this->setWindowFlags(Qt::Dialog);

  connect(m_ui->connectButton, SIGNAL(clicked(bool)), SLOT(connectToServer()));
  connect(m_ui->previewButton, SIGNAL(clicked(bool)), SLOT(setTiltAngle()));

  qDebug() << QImageReader::supportedImageFormats();

  vtkNew<vtkGenericOpenGLRenderWindow> window;
  m_ui->imageWidget->SetRenderWindow(window.Get());
  m_ui->imageWidget->GetRenderWindow()->AddRenderer(m_renderer.Get());
  m_ui->imageWidget->GetInteractor()->SetInteractorStyle(
    m_defaultInteractorStyle.Get());
  m_defaultInteractorStyle->SetRenderOnMouseMove(true);

  m_renderer->SetBackground(1.0, 1.0, 1.0);
  m_renderer->SetViewport(0.0, 0.0, 1.0, 1.0);
}

AcquisitionWidget::~AcquisitionWidget() = default;

void AcquisitionWidget::connectToServer()
{
  m_ui->statusEdit->setText("Attempting to connect to server...");
  m_client->setUrl("http://" + m_ui->hostnameEdit->text() + ":" +
                   m_ui->portEdit->text() + "/acquisition");
  auto request = m_client->connect(QJsonObject());
  connect(request, SIGNAL(finished(QJsonValue)), SLOT(onConnect()));
}

void AcquisitionWidget::onConnect()
{
  m_ui->statusEdit->setText("Connected to " + m_client->url() + "!!!");
  m_ui->connectButton->setEnabled(false);
  m_ui->disconnectButton->setEnabled(true);
  QJsonObject params;
  params["angle"] = 0.0;
  auto request = m_client->tilt_params(params);
  connect(request, SIGNAL(finished(QJsonValue)),
          SLOT(acquirePreview(QJsonValue)));
}

void AcquisitionWidget::setTiltAngle()
{
  QJsonObject params;
  params["angle"] = m_ui->tiltAngleSpinBox->value();
  auto request = m_client->tilt_params(params);
  connect(request, SIGNAL(finished(QJsonValue)),
          SLOT(acquirePreview(QJsonValue)));

  m_ui->previewButton->setEnabled(false);
  m_ui->acquireButton->setEnabled(false);
}

void AcquisitionWidget::acquirePreview(const QJsonValue& result)
{
  // This should be the actual angle the stage is at.
  if (result.isDouble()) {
    m_tiltAngle = result.toDouble(-69.99);
    m_ui->tiltAngle->setText(QString::number(result.toDouble(-69.99), 'g', 2));
  }

  auto request = m_client->preview_scan();
  connect(request, SIGNAL(finished(QString, QByteArray)),
          SLOT(previewReady(QString, QByteArray)));
}

void AcquisitionWidget::previewReady(QString mimeType, QByteArray result)
{
  qDebug() << "mimeType:" << mimeType;

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
  // file.open();
  file.write(result);
  qDebug() << "Data file:" << file.fileName();
  file.close();

  QImage image;
  QBuffer stream(&result);
  stream.open(QIODevice::ReadOnly);

  vtkNew<vtkTIFFReader> reader;
  reader->SetFileName(file.fileName().toLatin1());
  reader->Update();
  m_imageData = reader->GetOutput();
  m_imageSlice->GetProperty()->SetInterpolationTypeToNearest();
  m_imageSliceMapper->SetInputData(m_imageData.Get());
  m_imageSliceMapper->Update();
  m_imageSlice->SetMapper(m_imageSliceMapper.Get());
  m_renderer->AddViewProp(m_imageSlice.Get());
  resetCamera();
  m_ui->imageWidget->update();

  if (ActiveObjects::instance().activeDataSource()) {
    auto proxy = ActiveObjects::instance().activeDataSource()->colorMap();
    m_lut = vtkScalarsToColors::SafeDownCast(proxy->GetClientSideObject());
  } else {
    //    m_lut = vtkSmartPointer<vtkScalarsToColors>::New();
  }
  if (m_lut) {
    m_imageSlice->GetProperty()->SetLookupTable(m_lut.Get());
  }

  m_ui->previewButton->setEnabled(true);
  m_ui->acquireButton->setEnabled(true);

  if (image.load(&stream, "TIFF")) {
    // m_ui->image->setPixmap(QPixmap::fromImage(image));
  } else {
    qDebug() << "Failed to load image!";
    // qDebug() << result;
  }
}

void AcquisitionWidget::resetCamera()
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
}
