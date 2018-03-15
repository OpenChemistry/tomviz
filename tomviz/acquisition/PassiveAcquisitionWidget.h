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
#ifndef tomvizPassiveAcquisitionWidget_h
#define tomvizPassiveAcquisitionWidget_h

#include <QPointer>
#include <QScopedPointer>
#include <QString>
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
class QProcess;

namespace Ui {
class PassiveAcquisitionWidget;
}

namespace tomviz {

class AcquisitionClient;
class DataSource;

class PassiveAcquisitionWidget : public QWidget
{
  Q_OBJECT

public:
  PassiveAcquisitionWidget(QWidget* parent = nullptr);
  ~PassiveAcquisitionWidget() override;

protected:
  void closeEvent(QCloseEvent* event) override;

  void readSettings();
  void writeSettings();

private slots:
  void connectToServer(bool startServer=true);

  void imageReady(QString mimeType, QByteArray result, int angle = 0);

  void onError(const QString& errorMessage, const QJsonValue& errorData);
  void watchSource();
signals:
  void connectParameterDescription(QJsonValue params);

private:
  QString url() const;
  void introspectSource();
  QJsonObject connectParams();

  QVariantMap settings();

  QScopedPointer<Ui::PassiveAcquisitionWidget> m_ui;
  QScopedPointer<AcquisitionClient> m_client;

  vtkNew<vtkRenderer> m_renderer;
  vtkNew<vtkInteractorStyleRubberBand2D> m_defaultInteractorStyle;
  vtkSmartPointer<vtkImageData> m_imageData;
  vtkNew<vtkImageSlice> m_imageSlice;
  vtkNew<vtkImageSliceMapper> m_imageSliceMapper;
  vtkSmartPointer<vtkScalarsToColors> m_lut;

  DataSource* m_dataSource = nullptr;

  QString m_units = "unknown";
  double m_calX = 0.0;
  double m_calY = 0.0;
  QPointer<QWidget> m_connectParamsWidget;
  QPointer<QTimer> m_watchTimer;
  int m_retryCount = 5;
  QProcess* m_serverProcess = nullptr;

  void checkEnableWatchButton();
  void startLocalServer();
  void displayError(const QString& errorMessage);
  void setEnabledRegexGroupsWidget(bool enabled);
  void setEnabledRegexGroupsSubstitutionsWidget(bool enabled);
  void stopWatching();
};
}

#endif // tomvizPassiveAcquisitionWidget_h
