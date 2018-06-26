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

class QMenu;

namespace Ui {
class MainWindow;
}

namespace tomviz {

class AboutDialog;
class DataPropertiesPanel;
class DataSource;
class Module;
class Operator;
class OperatorResult;

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
  void openTilt();
  void openDataLink();
  void openUserGuide();
  void openVisIntro();

  /// Change the active data source in the UI.
  void dataSourceChanged(DataSource* source);

  /// Change the active module displayed in the UI.
  void moduleChanged(Module* module);

  /// Change the active module displayed in the properties panel.
  void operatorChanged(Operator* op);

  /// Change the active result displayed in the properties panel.
  void operatorResultChanged(OperatorResult* result);

  /// Load a custom operator from a file
  void importCustomTransform();

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
  QMenu* m_customTransformsMenu = nullptr;
  QTimer* m_timer = nullptr;
  bool m_isFirstShow = true;

  // Lazily loaded dialogs
  QWidget* m_aboutDialog = nullptr;
  QWidget* m_acquisitionWidget = nullptr;
  QWidget* m_passiveAcquisitionDialog = nullptr;

  template <class T>
  void openDialog(QWidget**);
};
} // namespace tomviz
#endif
