/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonTransform.h"

#include "CustomNodeWidgetRegistry.h"
#include "CustomPythonNodeWidget.h"
#include "ExternalNodeExecutor.h"
#include "ParameterBindingUtils.h"
#include "PythonNodeEditorWidget.h"

#include <QString>

namespace tomviz {
namespace pipeline {

PythonTransform::PythonTransform(QObject* parent) : TransformNode(parent)
{
}

void PythonTransform::setJSONDescription(const QString& json)
{
  m_backend.setJSONDescription(json);
  if (!m_backend.defaultLabel().isEmpty()) {
    setLabel(m_backend.defaultLabel());
  }
  setSupportsCancel(m_backend.supportsCancel());
  setSupportsCompletion(m_backend.supportsComplete());
  m_backend.applyDescription(
    [this](const QString& name, PortType type) {
      addInput(name, type);
    },
    [this](const QString& name, PortType type) {
      return addOutput(name, type);
    });

  // Default freshly created nodes to External execution when the
  // description demands it (externalOnly) or carries a legacy
  // tomviz_pipeline_env path.
  if (!nodeExecutor() &&
      (m_backend.externalOnly() ||
       !m_backend.externalPythonEnvPath().isEmpty())) {
    setNodeExecutor(
      new ExternalNodeExecutor(m_backend.externalPythonEnvPath()));
  }
}

QString PythonTransform::jsonDescription() const
{
  return m_backend.jsonDescription();
}

QStringList PythonTransform::reconfigureDescription(const QString& json)
{
  auto reset = m_backend.reconfigure(json);
  setSupportsCancel(m_backend.supportsCancel());
  setSupportsCompletion(m_backend.supportsComplete());
  return reset;
}

void PythonTransform::setScript(const QString& script)
{
  m_backend.setScript(script);
}

QString PythonTransform::scriptSource() const
{
  return m_backend.scriptSource();
}

void PythonTransform::setParameter(const QString& name, const QVariant& value)
{
  m_backend.setParameter(name, value);
}

QVariant PythonTransform::parameter(const QString& name) const
{
  return m_backend.parameter(name);
}

QMap<QString, QVariant> PythonTransform::parameters() const
{
  return m_backend.parameters();
}

void PythonTransform::applyParameterUpdates(const QVariantMap& updates)
{
  auto changed = m_backend.applyParameterUpdates(updates);
  if (!changed.isEmpty()) {
    emit parametersUpdated(changed);
  }
}

QString PythonTransform::operatorName() const
{
  return m_backend.operatorName();
}

QString PythonTransform::customWidgetID() const
{
  return m_backend.customWidgetID();
}

bool PythonTransform::hasPropertiesWidget() const
{
  return true;
}

EditNodeWidget* PythonTransform::createPropertiesWidget(Pipeline* pipeline,
                                                        QWidget* parent)
{
  PythonNodeEditorWidget::CustomWidgetFactory factory;
  bool customNeedsData = false;
  auto widgetID = m_backend.customWidgetID();
  const CustomWidgetInfo* info =
    widgetID.isEmpty() ? nullptr : findCustomNodeWidget(widgetID);
  if (info && info->create) {
    customNeedsData = info->needsData;
    factory = [this, info, pipeline](QWidget* p) -> CustomPythonNodeWidget* {
      auto* w = info->create(collectInputs(), p);
      if (w) {
        w->setScript(m_backend.scriptSource());
        w->setNodeContext(this, pipeline);
      }
      return w;
    };
  }

  QString currentType;
  QString currentEnvPath;
  if (auto* ext = qobject_cast<ExternalNodeExecutor*>(nodeExecutor())) {
    currentType = ExternalNodeExecutor::typeString();
    currentEnvPath = ext->envPath();
  }

  auto* widget = new PythonNodeEditorWidget(
    this, pipeline, label(), m_backend.scriptSource(),
    m_backend.jsonDescription(), m_backend.parameters(), currentType,
    currentEnvPath, factory, customNeedsData, parent);

  connect(widget, &PythonNodeEditorWidget::applied, this,
          [this](const PythonNodeEdits& edits) {
            bool changed = false;

            // Description first: it decides which parameters exist, so
            // the values below have to land on the new declarations.
            if (m_backend.jsonDescription() != edits.jsonDescription) {
              reconfigureDescription(edits.jsonDescription);
              changed = true;
            }

            if (label() != edits.label) {
              setLabel(edits.label);
              changed = true;
            }

            if (m_backend.scriptSource() != edits.script) {
              m_backend.setScript(edits.script);
              changed = true;
            }

            // Parameter values are already finalized by the editor.
            for (auto it = edits.values.constBegin();
                 it != edits.values.constEnd(); ++it) {
              if (m_backend.parameter(it.key()) != it.value()) {
                changed = true;
              }
              m_backend.setParameter(it.key(), it.value());
            }

            auto* currentExternal =
              qobject_cast<ExternalNodeExecutor*>(nodeExecutor());
            if (edits.executorType.isEmpty()) {
              if (nodeExecutor() != nullptr) {
                setNodeExecutor(nullptr);
                changed = true;
              }
            } else if (edits.executorType ==
                       ExternalNodeExecutor::typeString()) {
              if (currentExternal) {
                if (currentExternal->envPath() != edits.executorEnvPath) {
                  currentExternal->setEnvPath(edits.executorEnvPath);
                  changed = true;
                }
              } else {
                setNodeExecutor(
                  new ExternalNodeExecutor(edits.executorEnvPath));
                changed = true;
              }
            }

            // The auto-execute setting doesn't affect the node's data,
            // so it deliberately doesn't set `changed` — no pipeline
            // re-execution is warranted for toggling it. The setters
            // emit autoExecuteChanged for the controller.
            if (edits.autoExecuteEdited) {
              setAutoExecuteEnabled(edits.autoExecuteEnabled);
              setAutoExecuteIntervalSeconds(
                edits.autoExecuteIntervalSeconds);
            }

            if (changed) {
              emit parametersApplied();
            }
          });

  // Controls may be built late (once upstream data is in memory) or
  // rebuilt on Apply; bindings live on the controls, so re-wiring after
  // each build is safe. Same as LegacyPythonTransform.
  wireParameterBindings(this, widget, m_backend.parameterBindings());
  connect(widget, &PythonNodeEditorWidget::parameterWidgetInstalled, this,
          [this, widget]() {
            wireParameterBindings(this, widget,
                                  m_backend.parameterBindings());
          });

  return widget;
}

QJsonObject PythonTransform::serialize() const
{
  return m_backend.serializeInto(TransformNode::serialize());
}

bool PythonTransform::deserialize(const QJsonObject& json)
{
  // Apply description first so addInput / addOutput callbacks create
  // the ports before TransformNode::deserialize replays per-port
  // state from the saved JSON.
  m_backend.applySerializedFields(
    json,
    [this](const QString& name, PortType type) {
      addInput(name, type);
    },
    [this](const QString& name, PortType type) {
      return addOutput(name, type);
    });
  setSupportsCancel(m_backend.supportsCancel());
  setSupportsCompletion(m_backend.supportsComplete());
  if (!TransformNode::deserialize(json)) {
    return false;
  }
  // Legacy compatibility: an operator description may carry a
  // `tomviz_pipeline_env` field that pre-dates the per-node executor
  // serialization. Synthesize an ExternalNodeExecutor when set and
  // Node::deserialize didn't already install one.
  if (!nodeExecutor()) {
    auto envPath = m_backend.externalPythonEnvPath();
    if (!envPath.isEmpty() || m_backend.externalOnly()) {
      setNodeExecutor(new ExternalNodeExecutor(envPath));
    }
  }
  return true;
}

QMap<QString, PortData> PythonTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  return m_backend.runTransform(this, inputs);
}

bool PythonTransform::queryShouldAutoExecute()
{
  return m_backend.runShouldAutoExecute(this);
}

} // namespace pipeline
} // namespace tomviz
