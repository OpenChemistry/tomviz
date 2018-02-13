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
#ifndef tomvizWebExportWidget_h
#define tomvizWebExportWidget_h

#include <QDialog>
#include <QVariantMap>

#include "PythonUtilities.h"
#include "Utilities.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QWidget;

namespace tomviz {

class DataSource;
class SpinBox;

class WebExportWidget : public QDialog
{
  Q_OBJECT

public:
  WebExportWidget(QWidget* parent = nullptr);

  QMap<QString, QVariant>* getKeywordArguments();

private slots:
  void onBrowse();
  void onCancel();
  void onExport();
  void onPathChange();
  void onTypeChange(int);

private:
  QVariantMap readSettings();
  void writeSettings(const QVariantMap& settings);
  void writeWidgetSettings();
  void restoreSettings();

  QCheckBox* m_keepData;
  QComboBox* m_exportType;
  QLineEdit* m_outputPath;
  QLineEdit* m_multiValue;
  QPushButton* m_browseButton;
  QPushButton* m_cancelButton;
  QPushButton* m_exportButton;
  QSpinBox* m_imageHeight;
  QSpinBox* m_imageWidth;
  QSpinBox* m_maxOpacity;
  QSpinBox* m_nbPhi;
  QSpinBox* m_nbTheta;
  QSpinBox* m_scale;
  QSpinBox* m_spanValue;
  QWidget* m_cameraGroup;
  QWidget* m_imageSizeGroup;
  QWidget* m_valuesGroup;
  QWidget* m_volumeExplorationGroup;
  QWidget* m_volumeResampleGroup;

  QVariantMap m_kwargs;
};
}

#endif // tomvizWebExportWidget
