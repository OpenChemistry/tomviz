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
#ifndef tomvizAcquisitionWidget_h
#define tomvizAcquisitionWidget_h

#include <QScopedPointer>
#include <QWidget>

#include <vtkNew.h>
#include <vtkSmartPointer.h>

class vtkImageData;
class vtkImageSlice;
class vtkImageSliceMapper;
class vtkInteractorStyleRubberBand2D;
class vtkInteractorStyleRubberBandZoom;
class vtkRenderer;
class vtkScalarsToColors;

namespace Ui {
class AcquisitionWidget;
}

namespace tomviz {

class AcquisitionClient;

class AcquisitionWidget : public QWidget
{
  Q_OBJECT

public:
  AcquisitionWidget(QWidget* parent = nullptr);
  ~AcquisitionWidget() override;

protected:
  void closeEvent(QCloseEvent* event) override;

  void readSettings();
  void writeSettings();

private slots:
  void connectToServer();
  void onConnect();

  void disconnectFromServer();
  void onDisconnect();

  void setAcquireParameters();
  void acquireParameterResponse(const QJsonValue& result);

  void setTiltAngle();
  void acquirePreview(const QJsonValue& result);
  void previewReady(QString, QByteArray);

  void resetCamera();
  void onError(const QString& errorMessage, const QJsonValue& errorData);

private:
  QScopedPointer<Ui::AcquisitionWidget> m_ui;
  QScopedPointer<AcquisitionClient> m_client;

  vtkNew<vtkRenderer> m_renderer;
  vtkNew<vtkInteractorStyleRubberBand2D> m_defaultInteractorStyle;
  vtkSmartPointer<vtkImageData> m_imageData;
  vtkNew<vtkImageSlice> m_imageSlice;
  vtkNew<vtkImageSliceMapper> m_imageSliceMapper;
  vtkSmartPointer<vtkScalarsToColors> m_lut;

  double m_tiltAngle = 0.0;
  QString m_units = "unknown";
  double m_calX = 0.0;
  double m_calY = 0.0;
};
}

#endif // tomvizAcquisitionWidget_h
