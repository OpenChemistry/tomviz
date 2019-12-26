/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMainWindow_h
#define tomvizMainWindow_h

#include <QMainWindow>

#include <vector>

#include <QScopedPointer>

class QMenu;

namespace Ui {
class MainWindow;
}

namespace tomviz {

class AboutDialog;
class DataPropertiesPanel;
class DataSource;
class MoleculeSource;
class Module;
class Operator;
struct OperatorDescription;
class OperatorResult;

/// The main window for the tomviz application.
class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget* parent = nullptr, Qt::WindowFlags flags = nullptr);
  ~MainWindow() override;
  void openFiles(int argc, char** argv);

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
  void openReadTheDocs();
  void openUserGuide();
  void openVisIntro();

  /// Change the active data source in the UI.
  void dataSourceChanged(DataSource* source);

  /// Change the active molecule source in the UI.
  void moleculeSourceChanged(MoleculeSource* moleculeSource);

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

  void setEnabledPythonConsole(bool enabled);

private:
  Q_DISABLE_COPY(MainWindow)

  /// Find and register any user defined operators
  static std::vector<OperatorDescription> findCustomOperators();
  void registerCustomOperators(std::vector<OperatorDescription> operators);
  static std::vector<OperatorDescription> initPython();
  void syncPythonToApp();

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
