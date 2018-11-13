/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
} // namespace tomviz

#endif // tomvizAcquisitionWidget_h
