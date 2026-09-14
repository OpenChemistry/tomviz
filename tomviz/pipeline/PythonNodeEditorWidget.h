/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePythonNodeEditorWidget_h
#define tomvizPipelinePythonNodeEditorWidget_h

#include "EditNodeWidget.h"
#include "PortData.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <memory>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QTimer;
class QSpinBox;
class QTabWidget;
class QTextEdit;
class QVBoxLayout;
class QWidget;

namespace tomviz {
namespace pipeline {

class CustomPythonNodeWidget;
class InputsNotReadyWidget;
class Node;
class NodeDefinitionWidget;
class NodePropertiesWidget;
class Pipeline;
class PythonEnvironmentCheck;
struct PythonEnvironmentInfo;

/// Everything one Apply/OK commits back to a Python node, in the order
/// the node must apply it: the description first (it decides which
/// parameters exist at all), then the rest.
struct PythonNodeEdits
{
  QString label;
  QString script;
  QString jsonDescription;
  QMap<QString, QVariant> values;
  /// Empty for the default in-process executor, or the type string
  /// (e.g. "external") for an alternative.
  QString executorType;
  /// Type-specific executor configuration (currently the env path).
  QString executorEnvPath;
  /// True when the editor exposed the auto-execute controls (schema-v2
  /// nodes only). The two fields below are meaningful only then; a
  /// false value tells the node to leave its setting untouched.
  bool autoExecuteEdited = false;
  bool autoExecuteEnabled = false;
  int autoExecuteIntervalSeconds = 30;
};

/// Tabbed editor widget for Python source / transform nodes.
/// Tab 1: The node's raw JSON description — what the other tabs are
///        derived from, hence first. Editing it re-renders the
///        Parameters tab live, so the form on screen always matches the
///        description being edited rather than the one the node is
///        running. canApply() goes false while the description doesn't
///        validate, which is what disables the host's Apply/OK.
/// Tab 2: Python script editor with syntax highlighting.
/// Tab 3: Operator description + JSON-driven parameter controls, or a
///        custom widget if one is registered. When the custom widget
///        needs input data and the data isn't yet available, this tab
///        renders an InputsNotReadyWidget and swaps in the real custom
///        widget once the inputs materialize. Script and Execution tabs
///        remain fully usable in the meantime.
/// Tab 4: Execution strategy — Internal (default) or External (run via
///        the `tomviz-pipeline` CLI in a foreign Python env).
///
/// applyChangesToOperator() commits the label, script text, description,
/// parameter values, and execution-strategy choice back to the node.
/// Parameter values are pulled from whichever Parameters-tab widget is
/// currently installed; if the tab is the not-ready warning, no
/// parameter values are emitted (the existing values stay).
class PythonNodeEditorWidget : public EditNodeWidget
{
  Q_OBJECT

public:
  using CustomWidgetFactory =
    std::function<CustomPythonNodeWidget*(QWidget* parent)>;

  PythonNodeEditorWidget(
    Node* node, Pipeline* pipeline,
    const QString& label, const QString& script,
    const QString& jsonDescription,
    const QMap<QString, QVariant>& currentValues,
    const QString& executorType, const QString& executorEnvPath,
    CustomWidgetFactory customWidgetFactory,
    bool customWidgetNeedsData,
    QWidget* parent = nullptr);

  void applyChangesToOperator() override;

  /// Reads "help": {"url": ...} from the JSON description.
  QString helpUrl() const override;

  /// False while the Definition tab holds a description the node can't
  /// adopt, so the host's Apply/OK stays disabled until it is fixed or
  /// reverted.
  bool canApply() const override;

  /// Switch to the "Script" tab (used for "View Code" actions).
  void showScriptTab();

  /// Path of the JSON description that belongs beside @a scriptPath: same
  /// directory and stem, ".json" instead of ".py".
  static QString descriptionPathFor(const QString& scriptPath);

  /// Non-interactive save. Writes the script exactly as it stands in the
  /// editor to @a scriptPath, and (when @a withDescription) the description
  /// exactly as it stands in the Definition tab to descriptionPathFor().
  /// Overwrites without asking; saveScript() is the interactive wrapper
  /// that prompts and validates first. Returns false if the script could
  /// not be written.
  bool saveScriptTo(const QString& scriptPath, bool withDescription = true);

signals:
  /// Emitted by applyChangesToOperator() carrying everything the node
  /// should adopt.
  void applied(const PythonNodeEdits& edits);

  /// Emitted whenever the parameter controls are (re)built, including
  /// the deferred build once upstream data arrives, so callers can
  /// re-wire anything attached to individual controls.
  void parameterWidgetInstalled();

private:
  void onRunRequested();
  void onExecutionFinished();
  void installCustomWidget();
  void installJsonFormWidget();
  void installNotReadyWidget();
  void rebuildParametersTab(const QString& json);
  /// Tear down whatever the Parameters tab holds (form or not-ready
  /// widget) and install the right one for m_jsonDescription /
  /// m_currentValues.
  void reinstallParametersWidget();
  /// The node wrote back to its own parameters while running (kernel
  /// `self.set_parameter`): show the new values without discarding the
  /// user's other in-progress edits.
  void onNodeParametersUpdated(const QVariantMap& changed);
  /// Write the script text to a user-chosen .py file, with the current
  /// JSON description saved beside it as <name>.json. Both come from the
  /// editor's widgets, so unapplied edits are included.
  void saveScript();
  bool inputsInMemory() const;
  /// Debounced entry point for env-path edits: (re)starts the timer
  /// that triggers runEnvironmentCheck(), or clears the status when
  /// the Internal executor is selected.
  void scheduleEnvironmentCheck();
  /// Validate the env path asynchronously (PythonEnvironmentCheck)
  /// and show "Checking..." meanwhile.
  void runEnvironmentCheck();
  /// Show the verdict under the env row. When the path pointed at
  /// <env>/bin or the interpreter, rewrite it to the environment root.
  void showEnvironmentStatus(const PythonEnvironmentInfo& info);

  Node* m_node;
  Pipeline* m_pipeline;
  CustomWidgetFactory m_customFactory;
  bool m_customWidgetNeedsData;
  bool m_jsonFormNeedsData = false;
  // Description declared "externalOnly": Internal executor is disabled.
  bool m_externalOnly = false;
  // Description declared "externalCompatible": false — the operator's
  // imports only resolve in the application environment, so the
  // External executor is disabled.
  bool m_internalOnly = false;
  // JSON "name" field; keys the remembered external-env path.
  QString m_operatorName;
  QString m_jsonDescription;
  QMap<QString, QVariant> m_currentValues;

  QLineEdit* m_nameEdit = nullptr;
  QTabWidget* m_tabWidget = nullptr;
  QTextEdit* m_scriptEdit = nullptr;
  NodeDefinitionWidget* m_definitionWidget = nullptr;
  QWidget* m_paramsTab = nullptr;
  int m_scriptTabIndex = -1;
  int m_paramsTabIndex = -1;
  QVBoxLayout* m_paramsLayout = nullptr;
  NodePropertiesWidget* m_paramsWidget = nullptr;
  CustomPythonNodeWidget* m_customParamsWidget = nullptr;
  InputsNotReadyWidget* m_notReadyWidget = nullptr;
  QComboBox* m_executorCombo = nullptr;
  QLabel* m_envPathLabel = nullptr;
  QWidget* m_envPathRow = nullptr;
  QLineEdit* m_envPathEdit = nullptr;
  QLabel* m_envStatusLabel = nullptr;
  PythonEnvironmentCheck* m_envCheck = nullptr;
  QTimer* m_envCheckTimer = nullptr;
  QCheckBox* m_autoExecCheck = nullptr;
  QSpinBox* m_autoExecIntervalSpin = nullptr;

  /// Holds OnDisk-evicted upstream payloads in memory while the editor
  /// is shown, so the custom widget can read from the input ports.
  QList<std::shared_ptr<PortData>> m_inputPins;
};

} // namespace pipeline
} // namespace tomviz

#endif
