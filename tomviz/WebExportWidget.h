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

#include "PythonUtilities.h"
#include "Utilities.h"

class QButtonGroup;
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
  ~WebExportWidget();

  Python::Dict getKeywordArguments();

protected slots:
  void onBrowse();
  void onCancel();
  void onExport();
  void onPathChange();
  void onTypeChange(int);

protected:
  QWidget* cameraGroup;
  QLineEdit* outputPath;
  QPushButton* browseButton;
  QComboBox* exportType;
  QSpinBox* nbPhi;
  QSpinBox* nbTheta;
  QPushButton* exportButton;
  QPushButton* cancelButton;
};
}

#endif // tomvizWebExportWidget
