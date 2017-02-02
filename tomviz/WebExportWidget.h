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

class QLabel;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QTimer;
class QButtonGroup;
class QPushButton;

namespace tomviz {

class DataSource;
class SpinBox;

class WebExportWidget : public QDialog
{
  Q_OBJECT

public:
  WebExportWidget(QWidget* parent = nullptr);
  ~WebExportWidget();

  QString getOutputPath();
  int getExportType();
  int getDeltaPhi();
  int getDeltaTheta();

protected slots:
  void onPathChange();
  void onBrowse();
  void onCancel();
  void onExport();

protected:
  QLineEdit* outputPath;
  QPushButton* browseButton;
  QComboBox* exportType;
  QSpinBox* deltaPhi;
  QSpinBox* deltaTheta;
  QPushButton* exportButton;
  QPushButton* cancelButton;
};
}

#endif // tomvizWebExportWidget
