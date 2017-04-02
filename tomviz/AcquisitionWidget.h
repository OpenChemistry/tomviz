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

#include <QWidget>
#include <QScopedPointer>

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

private slots:
  void connectToServer();
  void onConnect();

  void setTiltAngle();
  void acquirePreview(const QJsonValue& result);
  void previewReady(QString, QByteArray);

  void resetCamera();

private:
  QScopedPointer<Ui::AcquisitionWidget> m_ui;
  QScopedPointer<AcquisitionClient> m_client;

  vtkNew<vtkRenderer> m_renderer;
  vtkNew<vtkInteractorStyleRubberBand2D> m_defaultInteractorStyle;
  vtkSmartPointer<vtkImageData> m_imageData;
  vtkNew<vtkImageSlice> m_imageSlice;
  vtkNew<vtkImageSliceMapper> m_imageSliceMapper;
  vtkSmartPointer<vtkScalarsToColors> m_lut;
  //vtkSmartPointer<vtkSMProxy> m_lut;
};

}

#endif // tomvizAcquisitionWidget_h
