/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMainWindow_h
#define tomvizMainWindow_h

#include <QMainWindow>

#include <vector>

#include <QPointer>
#include <QScopedPointer>

class QMenu;
class vtkVector3i;

namespace Ui {
class MainWindow;
}

namespace tomviz {

class AboutDialog;
class DataSource;
class Module;
class CustomOperatorEditDialog;
class CustomOperatorManagerDialog;
struct OperatorDescription;
class OperatorSearchDialog;
class ProgressDialogManager;

namespace pipeline {
class Link;
class Node;
class OutputPort;
class Pipeline;
class PipelineControlsWidget;
class PipelineStripWidget;
class SinkGroupNode;
class VolumePropertiesWidget;
} // namespace pipeline

/// The main window for the tomviz application.
class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
  ~MainWindow() override;
  void openFiles(int argc, char** argv);

  static MainWindow* instance();

  pipeline::Pipeline* pipeline() const;

  void setMostRecentStateFile(const QString& fileName);

protected:
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

  /// Check the system at runtime to see for an appropriate OpenGL version.
  bool checkOpenGL();

public slots:
  void openRecon();

private slots:
  void onNodeSelected(pipeline::Node* node);
  void onPortSelected(pipeline::OutputPort* port);
  void onLinkSelected(pipeline::Link* link);
  void onActiveNodeChanged(pipeline::Node* node);
  void onActivePortChanged(pipeline::OutputPort* port);
  void onActiveLinkChanged(pipeline::Link* link);
  void openTilt();
  void openDataLink();
  void openReadTheDocs();
  void openUserGuide();
  void openVisIntro();

  void onFirstWindowShow();

  void autosave();

  /// Save the state file to its most recent location
  void saveState();

  /// raise output widget on errors.
  void handleMessage(const QString&, int);

  void setEnabledPythonConsole(bool enabled);

  void onMouseOverVoxel(const vtkVector3i& ijk, double v);

  /// Load a custom pipeline template
  void findPipelineTemplates();

private:
  Q_DISABLE_COPY(MainWindow)

  /// Find and register any user defined operators
  static std::vector<OperatorDescription> findCustomOperators();
  void registerCustomOperators(std::vector<OperatorDescription> operators);
  /// Handlers for the Custom Transforms menu's "Create New..." and
  /// "Manage..." entries and for the Manage dialog's buttons.
  void createCustomOperator();
  void manageCustomOperators();
  void cloneCustomOperator(const OperatorDescription& op);
  void deleteCustomOperator(const OperatorDescription& op);
  void editCustomOperator(const OperatorDescription& op);
  void openCustomOperatorDirectory(const OperatorDescription& op);
  /// Show @a dialog modeless, or report why it could not load.
  void showCustomOperatorDialog(CustomOperatorEditDialog* dialog);
  static std::vector<OperatorDescription> initPython();
  void updateSaveStateEnableState();
  QString mostRecentStateFile() const;

  void initPipeline();
  void clearDynamicPropertiesWidget();
  /// Install @a content as the dynamic properties panel, wrapped with a
  /// title header declaring the type of the selected object.
  void showPropertiesPanel(QWidget* content, const QString& title);
  /// Relink a group member sink to the group's upstream port (removing it
  /// from the group).
  void leaveGroup(pipeline::Node* member, pipeline::SinkGroupNode* group);
  void updateColorMapDisplay();
  /// Coalescing wrapper around updateColorMapDisplay — at most one
  /// refresh per kColorMapUpdateThrottleMs window.
  void scheduleColorMapDisplayUpdate();
  /// Ensure @a port's VolumeData has a color map (init + copy from
  /// upstream on first use) and rescale it. Returns true on first
  /// creation so the caller can refresh sinks.
  bool ensureColorMapForPort(pipeline::Node* node,
                             pipeline::OutputPort* port);
  void setPipelineMutationEnabled(bool enabled);
  QScopedPointer<Ui::MainWindow> m_ui;
  QMenu* m_customTransformsMenu = nullptr;
  QPointer<CustomOperatorManagerDialog> m_customOperatorManager;
  QMenu* m_pipelineTemplates = nullptr;
  OperatorSearchDialog* m_operatorSearchDialog = nullptr;
  QTimer* m_timer = nullptr;
  bool m_isFirstShow = true;

  // New pipeline infrastructure
  pipeline::PipelineControlsWidget* m_pipelineControls = nullptr;
  pipeline::PipelineStripWidget* m_pipelineStrip = nullptr;
  pipeline::Pipeline* m_pipeline = nullptr;
  ProgressDialogManager* m_progressDialogManager = nullptr;
  QMetaObject::Connection m_tipDataChangedConn;
  QMetaObject::Connection m_tipMetadataChangedConn;
  bool m_colorMapUpdatePending = false;
  QMetaObject::Connection m_sinkColorMapChangedConn;
  QMetaObject::Connection m_editingChangedConn;
  QPointer<QWidget> m_dynamicPropertiesWidget;

  QString m_mostRecentStateFile;

  // Lazily loaded dialogs
  QWidget* m_aboutDialog = nullptr;
  QWidget* m_acquisitionWidget = nullptr;
  QWidget* m_animationHelperDialog = nullptr;

  template <class T>
  void openDialog(QWidget**);
};
} // namespace tomviz
#endif
