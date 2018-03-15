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

#ifndef tomvizStartServerDialog_h
#define tomvizStartServerDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class StartServerDialog;
}

namespace tomviz {

class StartServerDialog : public QDialog
{
  Q_OBJECT

public:
  explicit StartServerDialog(QWidget* parent = nullptr);
  ~StartServerDialog() override;
  QString pythonExecutablePath() { return m_pythonExecutablePath; };

private:
  QScopedPointer<Ui::StartServerDialog> m_ui;
  QString m_pythonExecutablePath;

  void readSettings();
  void writeSettings();
  void setPythonExecutablePath(const QString& path);
};
}

#endif
