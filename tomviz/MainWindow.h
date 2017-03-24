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
#ifndef tomvizMainWindow_h
#define tomvizMainWindow_h

#include <QMainWindow>

namespace tomviz {

class AboutDialog;
class DataPropertiesPanel;
class DataSource;
class Module;
class Operator;

/// The main window for the tomviz application.
class MainWindow : public QMainWindow
{
  Q_OBJECT

  typedef QMainWindow Superclass;

public:
  MainWindow(QWidget* parent = nullptr, Qt::WindowFlags flags = nullptr);
  virtual ~MainWindow();

protected:
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  /// Check the system at runtime to see for an appropriate OpenGL version.
  bool checkOpenGL();

public slots:
  void openRecon();

private slots:
  void showAbout();
  void openTilt();
  void openDataLink();
  void openUserGuide();

  /// Change the active data source in the UI.
  void dataSourceChanged(DataSource* source);

  /// Change the active module displayed in the UI.
  void moduleChanged(Module* module);

  /// Change the active module displayed in the properties panel.
  void operatorChanged(Operator* op);

  void onFirstWindowShow();

  void autosave();

private:
  Q_DISABLE_COPY(MainWindow)

  AboutDialog* m_aboutDialog = nullptr;
  class MWInternals;
  MWInternals* Internals;
};
}
#endif
