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

#include <QDialog>

#include <QLabel>
#include <QMap>
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

enum class TestRegexFormat {
  npDm3,
  pmDm3,
  npTiff,
  pmTiff,
  Custom,
  Advanced
};

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

  void imageReady(QString mimeType, QByteArray result, float angle = 0);

  void onError(const QString& errorMessage, const QJsonValue& errorData);
  void watchSource();

  void formatChanged(int);
  void formatTabChanged(int index);
  void testFileNameChanged(QString);
  void customFileRegex();

signals:
  void connectParameterDescription(QJsonValue params);

private:
  QScopedPointer<Ui::PassiveAcquisitionWidget> m_ui;
  QScopedPointer<AcquisitionClient> m_client;

  QRegExp m_fileNameRegex;
  QString m_testFileName;
  QString m_negChar;
  QString m_posChar;
  QString m_pythonFileNameRegex;

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

  void validateFileNameRegex();
  void setupTestTable();
  void resizeTestTable();
  void buildFileRegex(QString, QString, QString, QString, QString);
  void setupFileFormatCombo();

  QList<TestRegexFormat> makeDefaultFormatOrder() const;
  QMap<TestRegexFormat, QString> makeDefaultFileNames() const;
  QMap<TestRegexFormat, QString> makeDefaultLabels() const;
  QMap<TestRegexFormat, QStringList> makeDefaultRegexParams() const;

  const QList<TestRegexFormat> m_defaultFormatOrder;
  const QMap<TestRegexFormat, QString> m_defaultFileNames;
  const QMap<TestRegexFormat, QString> m_defaultFormatLabels;
  const QMap<TestRegexFormat, QStringList> m_defaultRegexParams;
};
} // namespace tomviz

#endif // tomvizPassiveAcquisitionWidget_h
