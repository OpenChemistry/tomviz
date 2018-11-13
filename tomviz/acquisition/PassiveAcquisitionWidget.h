/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPassiveAcquisitionWidget_h
#define tomvizPassiveAcquisitionWidget_h

#include <QDialog>

#include "MatchInfo.h"

#include <QLabel>
#include <QPointer>
#include <QScopedPointer>
#include <QString>

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

class PassiveAcquisitionWidget : public QDialog
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
  void connectToServer(bool startServer = true);

  void imageReady(QString mimeType, QByteArray result, float angle = 0,
                  bool hasAngle = false);

  void onError(const QString& errorMessage, const QJsonValue& errorData);
  void watchSource();

  void formatTabChanged(int index);
  void testFileNameChanged(QString);
  void onBasicFormatChanged();

  void onRegexChanged(QString);

signals:
  void connectParameterDescription(QJsonValue params);

private:
  QScopedPointer<Ui::PassiveAcquisitionWidget> m_ui;
  QScopedPointer<AcquisitionClient> m_client;

  QString m_testFileName;

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

  QString url() const;
  void introspectSource();
  QJsonObject connectParams();
  QVariantMap settings();
  void checkEnableWatchButton();
  void startLocalServer();
  void displayError(const QString& errorMessage);
  void stopWatching();
  void validateTestFileName();

  void setupTestTable();
  void resizeTestTable();
};
} // namespace tomviz

#endif // tomvizPassiveAcquisitionWidget_h
