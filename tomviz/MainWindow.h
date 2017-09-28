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

#include <QScopedPointer>

namespace Ui {
class MainWindow;
}

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

public:
  MainWindow(QWidget* parent = nullptr, Qt::WindowFlags flags = nullptr);
  ~MainWindow() override;

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

  /// raise output widget on errors.
  void handleMessage(const QString&, int);

private:
  Q_DISABLE_COPY(MainWindow)

  /// Find and register any user defined operators
  void registerCustomOperators(const QString& path);
  void registerCustomOperators();

  QScopedPointer<Ui::MainWindow> m_ui;
  QTimer* m_timer = nullptr;
  bool m_isFirstShow = true;
  AboutDialog* m_aboutDialog = nullptr;
};
}
#endif
