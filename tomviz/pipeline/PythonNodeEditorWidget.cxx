/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonNodeEditorWidget.h"

#include "CustomPythonNodeWidget.h"
#include "ExternalNodeExecutor.h"
#include "InputPort.h"
#include "InputsNotReadyWidget.h"
#include "Link.h"
#include "Node.h"
#include "NodeDefinitionValidator.h"
#include "NodeDefinitionWidget.h"
#include "NodePropertiesWidget.h"
#include "OutputPort.h"
#include "ParameterInterfaceBuilder.h"
#include "Pipeline.h"
#include "PythonEnvironmentCheck.h"
#include "PythonScriptEdit.h"
#include "SourceNode.h"
#include "Utilities.h"
#include "data/VolumeData.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QTimer>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

// Settings store for remembered external-env paths. The app's
// pqSettings when available; a plain QSettings in test harnesses that
// run without pqApplicationCore.
QSettings* executorSettings()
{
  if (auto* core = pqApplicationCore::instance()) {
    return core->settings();
  }
  static QSettings settings;
  return &settings;
}

QString envPathSettingsKey(const QString& operatorName)
{
  return QStringLiteral("externalEnvPaths/") + operatorName;
}

} // anonymous namespace

namespace tomviz {
namespace pipeline {

PythonNodeEditorWidget::PythonNodeEditorWidget(
  Node* node, Pipeline* pipeline,
  const QString& label, const QString& script, const QString& jsonDescription,
  const QMap<QString, QVariant>& currentValues, const QString& executorType,
  const QString& executorEnvPath, CustomWidgetFactory customWidgetFactory,
  bool customWidgetNeedsData, QWidget* parent)
  : EditNodeWidget(parent), m_node(node), m_pipeline(pipeline),
    m_customFactory(std::move(customWidgetFactory)),
    m_customWidgetNeedsData(customWidgetNeedsData),
    m_jsonDescription(jsonDescription),
    m_currentValues(currentValues)
{
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Name row
  auto* nameRow = new QWidget(this);
  auto* nameLayout = new QHBoxLayout(nameRow);
  nameLayout->setContentsMargins(5, 5, 5, 5);
  nameLayout->setSpacing(5);
  nameLayout->addWidget(new QLabel(tr("Name"), nameRow));
  m_nameEdit = new QLineEdit(label, nameRow);
  nameLayout->addWidget(m_nameEdit);
  mainLayout->addWidget(nameRow);

  // Tab widget
  m_tabWidget = new QTabWidget(this);
  mainLayout->addWidget(m_tabWidget, 1);

  // --- Tab 1: Definition ---
  // First, because every other tab is derived from it. The node's C++
  // shell is fixed at construction, so the validator needs to know
  // which one it is serving; a source shell has no way to grow input
  // ports whatever its description says. Signals are connected further
  // down, once the Parameters tab it drives exists.
  //
  // Not offered when a custom widget owns the Parameters tab. Re-
  // rendering it from an edited description means the widget picking the
  // change up through setJSONDescription(), and no custom widget
  // implements that yet, so the tab would show controls built from the
  // old description while the editor committed the new one.
  if (!m_customFactory) {
    NodeShape shape = qobject_cast<SourceNode*>(m_node)
                        ? NodeShape::Source
                        : NodeShape::Transform;
    m_definitionWidget = new NodeDefinitionWidget(
      m_jsonDescription, shape, definitionSchema(m_jsonDescription),
      m_tabWidget);
    m_tabWidget->addTab(m_definitionWidget, tr("Definition"));
  }

  // --- Tab 2: Script ---
  auto* scriptTab = new QWidget(m_tabWidget);
  auto* scriptLayout = new QVBoxLayout(scriptTab);

  m_scriptEdit = new PythonScriptEdit(scriptTab);

  if (!script.isEmpty()) {
    m_scriptEdit->setPlainText(script);
  }

  scriptLayout->addWidget(m_scriptEdit, 1);

  m_scriptTabIndex = m_tabWidget->addTab(scriptTab, tr("Script"));

  // --- Tab 3: Parameters ---
  m_paramsTab = new QWidget(m_tabWidget);
  m_paramsLayout = new QVBoxLayout(m_paramsTab);

  // Modes (custom widget vs JSON form, with or without data dependency):
  //   1) No custom widget factory  → auto-form from JSON description.
  //      A "select_scalars" parameter in the JSON makes the form depend
  //      on input data (we need the list of scalar arrays from the
  //      upstream VolumeData to populate the picker).
  //   2) Custom widget, no data needed (or data already available)
  //      → build the custom widget immediately.
  //   3) Either path needs data and inputs aren't in memory yet
  //      → install InputsNotReadyWidget; swap to the real widget on
  //        the first executionFinished that delivers the data.
  if (!m_customFactory) {
    if (!jsonDescription.isEmpty()) {
      QJsonDocument doc = QJsonDocument::fromJson(jsonDescription.toUtf8());
      if (doc.isObject()) {
        for (const auto& p : doc.object().value("parameters").toArray()) {
          if (p.toObject().value("type").toString() == "select_scalars") {
            m_jsonFormNeedsData = true;
            break;
          }
        }
      }
    }
  }

  if (!m_customFactory) {
    if (jsonDescription.isEmpty()) {
      m_paramsLayout->addStretch();
    } else if (!m_jsonFormNeedsData || inputsInMemory()) {
      installJsonFormWidget();
    } else {
      installNotReadyWidget();
    }
  } else if (!m_customWidgetNeedsData || inputsInMemory()) {
    installCustomWidget();
  } else {
    installNotReadyWidget();
  }

  // Connected unconditionally: editing the description can introduce a
  // "select_scalars" parameter, so a form that didn't need input data
  // when the editor opened may need it a keystroke later. Both handlers
  // no-op while the not-ready widget isn't installed.
  connect(m_pipeline, &Pipeline::executionStarted, this, [this]() {
    if (m_notReadyWidget) {
      m_notReadyWidget->setRunEnabled(false);
    }
  });
  connect(m_pipeline, &Pipeline::executionFinished,
          this, &PythonNodeEditorWidget::onExecutionFinished);
  // Auto connection: the node emits this from the pipeline worker
  // thread mid-run, so the refresh is queued onto the GUI thread.
  connect(m_node, &Node::parametersUpdated, this,
          &PythonNodeEditorWidget::onNodeParametersUpdated);

  m_paramsTabIndex = m_tabWidget->addTab(m_paramsTab, tr("Parameters"));

  // --- Tab 4: Execution ---
  // Picks the per-node executor strategy. Empty string == Internal.
  if (!jsonDescription.isEmpty()) {
    QJsonDocument descDoc = QJsonDocument::fromJson(jsonDescription.toUtf8());
    if (descDoc.isObject()) {
      m_externalOnly =
        descDoc.object().value("externalOnly").toBool(false);
      // A contradictory externalOnly + externalCompatible=false combo is
      // treated as externalOnly, matching PythonNodeBackend.
      m_internalOnly =
        !descDoc.object().value("externalCompatible").toBool(true) &&
        !m_externalOnly;
      m_operatorName = descDoc.object().value("name").toString();
    }
  }
  auto* execTab = new QWidget(m_tabWidget);
  auto* execLayout = new QVBoxLayout(execTab);

  auto* execGridContainer = new QWidget(execTab);
  auto* execGrid = new QGridLayout(execGridContainer);
  execGrid->setContentsMargins(0, 0, 0, 0);

  auto* executorLabel = new QLabel(tr("Executor"), execGridContainer);
  m_executorCombo = new QComboBox(execGridContainer);
  m_executorCombo->setObjectName("executorTypeCombo");
  m_executorCombo->addItem(tr("Internal"), QString());
  m_executorCombo->addItem(
    tr("External"), ExternalNodeExecutor::typeString());
  if (m_externalOnly) {
    if (auto* model =
          qobject_cast<QStandardItemModel*>(m_executorCombo->model())) {
      model->item(0)->setEnabled(false);
    }
    m_executorCombo->setToolTip(
      tr("This operator requires an external Python environment"));
  } else if (m_internalOnly) {
    if (auto* model =
          qobject_cast<QStandardItemModel*>(m_executorCombo->model())) {
      model->item(1)->setEnabled(false);
    }
    m_executorCombo->setToolTip(
      tr("This operator can only run in the application's Python "
         "environment"));
  }
  executorLabel->setBuddy(m_executorCombo);
  execGrid->addWidget(executorLabel, 0, 0);
  execGrid->addWidget(m_executorCombo, 0, 1);

  m_envPathLabel = new QLabel(tr("Python Env"), execGridContainer);
  m_envPathRow = new QWidget(execGridContainer);
  auto* envLayout = new QHBoxLayout(m_envPathRow);
  envLayout->setContentsMargins(0, 0, 0, 0);
  // A node with no configured environment starts with the one last
  // applied for this operator type, so per-operator env paths (e.g.
  // the SAM 2 / SAM 3 conda envs) only need to be picked once.
  QString envPath = executorEnvPath;
  if (envPath.isEmpty() && !m_operatorName.isEmpty()) {
    envPath =
      executorSettings()->value(envPathSettingsKey(m_operatorName)).toString();
  }
  m_envPathEdit = new QLineEdit(envPath, m_envPathRow);
  m_envPathEdit->setObjectName("executorEnvPathEdit");
  m_envPathEdit->setPlaceholderText(
    tr("Path to a Python env containing tomviz-pipeline"));
  envLayout->addWidget(m_envPathEdit, 1);
  auto* browseBtn = new QPushButton(tr("Browse"), m_envPathRow);
  envLayout->addWidget(browseBtn);
  m_envPathLabel->setBuddy(m_envPathRow);
  execGrid->addWidget(m_envPathLabel, 1, 0);
  execGrid->addWidget(m_envPathRow, 1, 1);

  // Verdict of the environment check (is it an env, is tomviz-pipeline
  // installed, is its version compatible). Advisory only: Apply stays
  // enabled so the user can fix the environment afterwards.
  m_envStatusLabel = new QLabel(execGridContainer);
  m_envStatusLabel->setObjectName("executorEnvStatusLabel");
  m_envStatusLabel->setWordWrap(true);
  m_envStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_envStatusLabel->hide();
  execGrid->addWidget(m_envStatusLabel, 2, 1);

  m_envCheck = new PythonEnvironmentCheck(this);
  connect(m_envCheck, &PythonEnvironmentCheck::finished, this,
          &PythonNodeEditorWidget::showEnvironmentStatus);
  m_envCheckTimer = new QTimer(this);
  m_envCheckTimer->setSingleShot(true);
  m_envCheckTimer->setInterval(400);
  connect(m_envCheckTimer, &QTimer::timeout, this,
          &PythonNodeEditorWidget::runEnvironmentCheck);
  connect(m_envPathEdit, &QLineEdit::textChanged, this,
          &PythonNodeEditorWidget::scheduleEnvironmentCheck);

  // Row 3: periodic execution — schema-v2 nodes only.
  // The legacy (v1) operator API has no should_auto_execute hook, so
  // the controls are omitted entirely rather than shown disabled.
  // Schema is frozen for a live node (the Definition-tab validator
  // rejects v1↔v2 migration), so this can't become wrong mid-edit.
  if (definitionSchema(m_jsonDescription) == DefinitionSchema::V2) {
    auto* autoExecLabel =
      new QLabel(tr("Periodic Execution"), execGridContainer);
    auto* autoExecRow = new QWidget(execGridContainer);
    auto* autoExecLayout = new QHBoxLayout(autoExecRow);
    autoExecLayout->setContentsMargins(0, 0, 0, 0);
    m_autoExecCheck = new QCheckBox(tr("every"), autoExecRow);
    m_autoExecCheck->setObjectName("autoExecuteCheck");
    m_autoExecIntervalSpin = new QSpinBox(autoExecRow);
    m_autoExecIntervalSpin->setObjectName("autoExecuteIntervalSpin");
    m_autoExecIntervalSpin->setRange(1, 86400);
    m_autoExecIntervalSpin->setSuffix(tr(" s"));
    m_autoExecIntervalSpin->setValue(m_node->autoExecuteIntervalSeconds());
    m_autoExecCheck->setChecked(m_node->autoExecuteEnabled());
    m_autoExecIntervalSpin->setEnabled(m_autoExecCheck->isChecked());
    const QString autoExecTip =
      tr("Periodically check whether this node should re-execute.");
    m_autoExecCheck->setToolTip(autoExecTip);
    m_autoExecIntervalSpin->setToolTip(autoExecTip);
    connect(m_autoExecCheck, &QCheckBox::toggled,
            m_autoExecIntervalSpin, &QWidget::setEnabled);
    autoExecLayout->addWidget(m_autoExecCheck);
    autoExecLayout->addWidget(m_autoExecIntervalSpin);
    autoExecLayout->addStretch();
    autoExecLabel->setBuddy(autoExecRow);
    execGrid->addWidget(autoExecLabel, 3, 0);
    execGrid->addWidget(autoExecRow, 3, 1);
  }

  execLayout->addWidget(execGridContainer);
  execLayout->addStretch();
  m_tabWidget->addTab(execTab, tr("Execution"));

  int typeIdx = m_executorCombo->findData(executorType);
  if (typeIdx < 0 || (m_externalOnly && typeIdx == 0) ||
      (m_internalOnly && typeIdx == 1)) {
    typeIdx = m_externalOnly
      ? m_executorCombo->findData(ExternalNodeExecutor::typeString())
      : 0;
  }
  m_executorCombo->setCurrentIndex(typeIdx);
  // Enable/disable rather than show/hide so the grid columns don't
  // resize when toggling between Internal and External. Derive the
  // state from the combo, not executorType: an externalOnly node with
  // no configured executor still shows External selected.
  QString effectiveType = m_executorCombo->currentData().toString();
  m_envPathLabel->setEnabled(!effectiveType.isEmpty());
  m_envPathRow->setEnabled(!effectiveType.isEmpty());

  connect(m_executorCombo, &QComboBox::currentIndexChanged, this,
          [this](int) {
            QString type = m_executorCombo->currentData().toString();
            m_envPathLabel->setEnabled(!type.isEmpty());
            m_envPathRow->setEnabled(!type.isEmpty());
            scheduleEnvironmentCheck();
          });
  connect(browseBtn, &QPushButton::clicked, this, [this]() {
    auto dir = QFileDialog::getExistingDirectory(
      this, tr("Select Python environment"), m_envPathEdit->text());
    if (!dir.isEmpty()) {
      m_envPathEdit->setText(dir);
    }
  });
  // Validate whatever the editor opened with (a node loaded from a
  // state file may name an environment this machine doesn't have).
  runEnvironmentCheck();

  if (m_definitionWidget) {
    connect(m_definitionWidget, &NodeDefinitionWidget::validityChanged, this,
            [this](bool) { emit canApplyChanged(); });
    connect(m_definitionWidget, &NodeDefinitionWidget::parameterSchemaChanged,
            this, &PythonNodeEditorWidget::rebuildParametersTab);
  }

  m_tabWidget->setCurrentIndex(m_paramsTabIndex);
}

bool PythonNodeEditorWidget::canApply() const
{
  return !m_definitionWidget || m_definitionWidget->isValid();
}

void PythonNodeEditorWidget::scheduleEnvironmentCheck()
{
  if (m_executorCombo->currentData().toString().isEmpty()) {
    m_envCheckTimer->stop();
    m_envCheck->abort();
    m_envStatusLabel->hide();
    return;
  }
  m_envCheckTimer->start();
}

void PythonNodeEditorWidget::runEnvironmentCheck()
{
  m_envCheckTimer->stop();
  QString path = m_envPathEdit->text().trimmed();
  if (m_executorCombo->currentData().toString().isEmpty() ||
      path.isEmpty()) {
    m_envCheck->abort();
    m_envStatusLabel->hide();
    return;
  }
  // Same box geometry as the verdict styles so the label doesn't
  // jump when the result replaces this.
  m_envStatusLabel->setStyleSheet(
    "QLabel { color: palette(mid); background: palette(alternate-base); "
    "border: 1px solid palette(mid); border-radius: 4px; padding: 8px; }");
  m_envStatusLabel->setText(tr("Checking environment..."));
  m_envStatusLabel->show();
  m_envCheck->start(path);
}

void PythonNodeEditorWidget::showEnvironmentStatus(
  const PythonEnvironmentInfo& info)
{
  if (info.status == PythonEnvironmentInfo::Status::NoPath) {
    m_envStatusLabel->hide();
    return;
  }

  // A pick of <env>/bin or of the interpreter resolved to a root:
  // keep the canonical root in the field (silently — the verdict
  // being shown already covers it).
  QString typed = m_envPathEdit->text().trimmed();
  if (!info.envPath.isEmpty() && !typed.isEmpty() &&
      QDir::cleanPath(QFileInfo(typed).absoluteFilePath()) != info.envPath) {
    QSignalBlocker blocker(m_envPathEdit);
    m_envPathEdit->setText(info.envPath);
  }

  // Problems use the same amber warning style as InputsNotReadyWidget
  // (the verdict is advisory, not a blocking error); success gets its
  // green counterpart.
  m_envStatusLabel->setStyleSheet(
    info.ok()
      ? "QLabel { color: #15803d; background: #dcfce7; "
        "border: 1px solid #86efac; border-radius: 4px; padding: 8px; }"
      : "QLabel { color: #b45309; background: #fef3c7; "
        "border: 1px solid #fcd34d; border-radius: 4px; padding: 8px; }");
  m_envStatusLabel->setText(info.message);
  m_envStatusLabel->show();
}

QString PythonNodeEditorWidget::helpUrl() const
{
  QJsonDocument doc = QJsonDocument::fromJson(m_jsonDescription.toUtf8());
  if (!doc.isObject()) {
    return QString();
  }
  return doc.object()
    .value("help").toObject()
    .value("url").toString();
}

bool PythonNodeEditorWidget::inputsInMemory() const
{
  for (auto* input : m_node->inputPorts()) {
    if (!input->link() || input->isStale() || !input->hasData()) {
      return false;
    }
    if (input->link()->from()->dataLocation() != DataLocation::InMemory) {
      return false;
    }
  }
  return true;
}

void PythonNodeEditorWidget::installCustomWidget()
{
  m_customParamsWidget = m_customFactory(m_paramsTab);
  if (!m_customParamsWidget) {
    return;
  }
  // The factory (built by the node shell) seeds the script from the
  // node; hand it the description from here instead, so a widget that
  // renders any of its UI from it sees the copy being edited rather
  // than the one the node is still running.
  m_customParamsWidget->setJSONDescription(m_jsonDescription);
  m_customParamsWidget->setValues(m_currentValues);
  m_paramsLayout->addWidget(m_customParamsWidget, 1);
  emit parameterWidgetInstalled();
}

void PythonNodeEditorWidget::installJsonFormWidget()
{
  QList<PortScalars> portScalars;
  for (auto* input : m_node->inputPorts()) {
    if (!input->hasData()) {
      continue;
    }
    auto portData = input->data();
    if (!portData.isValid() || !isVolumeType(portData.type())) {
      continue;
    }
    auto vol = portData.value<VolumeDataPtr>();
    if (!vol || !vol->isValid()) {
      continue;
    }
    portScalars.append(
      { input->name(), vol->scalarNames(), vol->activeScalarName() });
  }

  m_paramsWidget = new NodePropertiesWidget(
    m_jsonDescription, m_currentValues, portScalars, m_paramsTab);
  m_paramsLayout->addWidget(m_paramsWidget, 1);
  m_paramsLayout->addStretch();
  emit parameterWidgetInstalled();
}

void PythonNodeEditorWidget::rebuildParametersTab(const QString& json)
{
  // A custom widget owns the whole tab, and the "widget" key is frozen,
  // so the tab itself never has to be rebuilt. Push the new description
  // at it instead: widgets that render part of their UI from the
  // description (via NodePropertiesWidget or ParameterInterfaceBuilder)
  // override setJSONDescription and re-render, and the rest ignore it.
  if (m_customFactory) {
    m_jsonDescription = json;
    if (m_customParamsWidget) {
      m_customParamsWidget->setJSONDescription(json);
    }
    return;
  }

  // Keep what the user has typed into the form, but only where the new
  // description still declares the parameter at the same type — a
  // retyped or dropped parameter has to go back to its default, exactly
  // as PythonNodeBackend::reconfigure will decide at apply time.
  QMap<QString, QVariant> live;
  if (m_paramsWidget) {
    live = m_paramsWidget->values();
  }
  for (auto it = live.constBegin(); it != live.constEnd(); ++it) {
    m_currentValues[it.key()] = it.value();
  }
  auto previousTypes = parameterDeclaredTypes(m_jsonDescription);
  auto newTypes = parameterDeclaredTypes(json);
  const auto names = m_currentValues.keys();
  for (const auto& name : names) {
    if (!newTypes.contains(name) ||
        newTypes.value(name) != previousTypes.value(name)) {
      m_currentValues.remove(name);
    }
  }

  m_jsonDescription = json;

  m_jsonFormNeedsData = false;
  QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
  for (const auto& p : doc.object().value("parameters").toArray()) {
    if (p.toObject().value("type").toString() == "select_scalars") {
      m_jsonFormNeedsData = true;
      break;
    }
  }

  reinstallParametersWidget();
}

void PythonNodeEditorWidget::reinstallParametersWidget()
{
  while (QLayoutItem* item = m_paramsLayout->takeAt(0)) {
    if (auto* widget = item->widget()) {
      widget->hide();
      widget->setParent(nullptr);
      widget->deleteLater();
    }
    delete item;
  }
  m_paramsWidget = nullptr;
  m_notReadyWidget = nullptr;

  if (!m_jsonFormNeedsData || inputsInMemory()) {
    installJsonFormWidget();
  } else {
    installNotReadyWidget();
  }
}

void PythonNodeEditorWidget::onNodeParametersUpdated(const QVariantMap& changed)
{
  if (m_customFactory) {
    for (auto it = changed.constBegin(); it != changed.constEnd(); ++it) {
      m_currentValues[it.key()] = it.value();
    }
    if (m_customParamsWidget) {
      m_customParamsWidget->setValues(m_currentValues);
    }
    return;
  }

  // Keep what the user has typed into the form, then let the node's
  // write-backs win for the parameters it changed. (Deliberately not
  // rebuildParametersTab(): that merges the live form values *last*,
  // which would put the stale ones back.)
  if (m_paramsWidget) {
    const auto live = m_paramsWidget->values();
    for (auto it = live.constBegin(); it != live.constEnd(); ++it) {
      m_currentValues[it.key()] = it.value();
    }
  }
  for (auto it = changed.constBegin(); it != changed.constEnd(); ++it) {
    m_currentValues[it.key()] = it.value();
  }
  if (!m_paramsWidget) {
    // The not-ready widget is up; the form picks the values up when
    // it is eventually built from m_currentValues.
    return;
  }
  reinstallParametersWidget();
}

void PythonNodeEditorWidget::installNotReadyWidget()
{
  m_notReadyWidget = new InputsNotReadyWidget(m_paramsTab);
  m_notReadyWidget->setRunEnabled(!m_pipeline->isExecuting());
  connect(m_notReadyWidget, &InputsNotReadyWidget::runRequested,
          this, &PythonNodeEditorWidget::onRunRequested);
  m_paramsLayout->addWidget(m_notReadyWidget, 1);
}

void PythonNodeEditorWidget::onRunRequested()
{
  m_pipeline->executeUpstreamOf(m_node);
}

void PythonNodeEditorWidget::onExecutionFinished()
{
  if (!m_notReadyWidget) {
    return;
  }

  for (auto* input : m_node->inputPorts()) {
    auto* link = input->link();
    if (!link || !link->from()) {
      continue;
    }
    if (auto handle = link->from()->materialize()) {
      m_inputPins.append(handle);
    }
  }

  if (!inputsInMemory()) {
    m_notReadyWidget->setRunEnabled(true);
    return;
  }

  m_paramsLayout->removeWidget(m_notReadyWidget);
  m_notReadyWidget->deleteLater();
  m_notReadyWidget = nullptr;
  if (m_customFactory) {
    installCustomWidget();
  } else {
    installJsonFormWidget();
  }
}

void PythonNodeEditorWidget::applyChangesToOperator()
{
  // Validate whatever is on screen right now: Apply may well have been
  // clicked before the Definition tab's debounce fired, in which case
  // canApply() still reflects the previous keystroke. Flushing can also
  // rebuild the Parameters tab, so it has to happen before the values
  // below are harvested from it.
  if (m_definitionWidget) {
    m_definitionWidget->flushPendingValidation();
  }
  if (!canApply()) {
    QMessageBox::warning(
      this, tr("Invalid node definition"),
      tr("The description in the Definition tab can't be applied to this "
         "node. Fix the problems listed there, or cancel to discard the "
         "edit."));
    return;
  }

  PythonNodeEdits edits;
  if (m_customParamsWidget) {
    m_customParamsWidget->getValues(edits.values);
    m_customParamsWidget->writeSettings();
  } else if (m_paramsWidget) {
    edits.values = m_paramsWidget->values();
  }
  // If neither is set (Parameters tab is the not-ready warning), emit
  // an empty values map — the Python node leaves existing parameters
  // unchanged but still picks up Script and Execution edits.
  edits.label = m_nameEdit->text();
  edits.script = m_scriptEdit->toPlainText();
  edits.jsonDescription =
    m_definitionWidget ? m_definitionWidget->definitionText()
                       : m_jsonDescription;
  edits.executorType = m_executorCombo->currentData().toString();
  edits.executorEnvPath =
    edits.executorType.isEmpty() ? QString() : m_envPathEdit->text();
  // Store the environment root even when Apply came before the
  // debounced check could rewrite a <env>/bin or interpreter pick.
  QString envRoot =
    PythonEnvironmentCheck::resolveEnvironmentRoot(edits.executorEnvPath);
  if (!envRoot.isEmpty()) {
    edits.executorEnvPath = envRoot;
  }
  if (m_autoExecCheck && m_autoExecIntervalSpin) {
    edits.autoExecuteEdited = true;
    edits.autoExecuteEnabled = m_autoExecCheck->isChecked();
    edits.autoExecuteIntervalSeconds = m_autoExecIntervalSpin->value();
  }

  // The Definition tab can rewrite both "externalOnly" and "name", so
  // re-read them from what is being committed rather than from what the
  // editor opened on — otherwise a renamed operator would file its
  // remembered environment under the old name.
  QJsonObject descObj =
    QJsonDocument::fromJson(edits.jsonDescription.toUtf8()).object();
  m_externalOnly = descObj.value("externalOnly").toBool(false);
  m_internalOnly =
    !descObj.value("externalCompatible").toBool(true) && !m_externalOnly;
  m_operatorName = descObj.value("name").toString();

  if (m_internalOnly && !edits.executorType.isEmpty()) {
    // The (possibly just-edited) description says in-app only
    edits.executorType.clear();
    edits.executorEnvPath.clear();
    m_executorCombo->setCurrentIndex(0);
  }

  // Remember the applied environment per operator type so future
  // instances of this operator start with it prefilled.
  if (!edits.executorType.isEmpty() && !edits.executorEnvPath.isEmpty() &&
      !m_operatorName.isEmpty()) {
    executorSettings()->setValue(envPathSettingsKey(m_operatorName),
                                 edits.executorEnvPath);
  }

  if (m_externalOnly && edits.executorEnvPath.isEmpty()) {
    QMessageBox::warning(
      this, tr("External environment required"),
      tr("This operator only runs in an external Python environment, but "
         "none is selected. It will fail until you choose an environment "
         "containing tomviz-pipeline (plus the operator's dependencies) "
         "in the Execution tab."));
  }

  emit applied(edits);

  // The node has adopted this description, so it becomes the baseline
  // the Definition tab validates later edits against.
  if (m_definitionWidget) {
    m_jsonDescription = edits.jsonDescription;
    m_definitionWidget->markApplied(edits.jsonDescription);
  }
}

QString PythonNodeEditorWidget::descriptionPathFor(const QString& scriptPath)
{
  // completeBaseName() strips only the final suffix, so "my.operator.py"
  // stays "my.operator" rather than collapsing to "my".
  const QFileInfo info(scriptPath);
  return info.dir().filePath(info.completeBaseName() +
                             QLatin1String(".json"));
}

QString PythonNodeEditorWidget::scriptText() const
{
  return m_scriptEdit->toPlainText();
}

QString PythonNodeEditorWidget::definitionText() const
{
  return m_definitionWidget ? m_definitionWidget->definitionText()
                            : m_jsonDescription;
}

QString PythonNodeEditorWidget::nodeLabel() const
{
  return m_nameEdit->text();
}

void PythonNodeEditorWidget::showScriptTab()
{
  m_tabWidget->setCurrentIndex(m_scriptTabIndex);
}

} // namespace pipeline
} // namespace tomviz
