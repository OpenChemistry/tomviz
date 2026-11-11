/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TransformNode.h"

#include "InputPort.h"
#include "OutputPort.h"
#include "PipelineSettings.h"
#include "PortDataMetadata.h"

#include <QDebug>

namespace tomviz {
namespace pipeline {

TransformNode::TransformNode(QObject* parent) : Node(parent) {}

QIcon TransformNode::icon() const
{
  return QIcon(
    QStringLiteral(":/pqWidgets/Icons/pqProgrammableFilter.svg"));
}

InputPort* TransformNode::addInput(const QString& name,
                                   PortTypes acceptedTypes)
{
  return addInputPort(name, acceptedTypes);
}

OutputPort* TransformNode::addOutput(const QString& name, PortType type)
{
  auto* port = addOutputPort(name, type);
  applyDefaultPersistence(port);
  return port;
}

void TransformNode::applyDefaultPersistence(OutputPort* port)
{
  if (!port) {
    return;
  }
  // Defer to the application-wide default. Schema-v2 / explicit
  // setPersistent() callers (e.g. operator JSON with `persistent: true`)
  // still override afterwards.
  switch (PipelineSettings::instance().transformPersistenceDefault()) {
    case TransformPersistenceDefault::InMemory:
      // Set the mode before enabling persistence so the single
      // reconcile triggered by setPersistent runs with the new mode
      // already in place — avoids an InMemory acquire/evict round
      // trip when the target mode is OnDisk.
      port->setPersistenceMode(PersistenceMode::InMemory);
      port->setPersistent(true);
      break;
    case TransformPersistenceDefault::OnDisk:
      port->setPersistenceMode(PersistenceMode::OnDisk);
      port->setPersistent(true);
      break;
    case TransformPersistenceDefault::Transient:
      // Default-constructed port is already transient; nothing to do.
      break;
  }
}

QJsonObject TransformNode::serialize() const
{
  return Node::serialize();
}

bool TransformNode::deserialize(const QJsonObject& json)
{
  return Node::deserialize(json);
}

bool TransformNode::execute()
{
  resetExecutionFlags();
  resetProgress();
  setExecState(NodeExecState::Running);

  // An operator gets an input port per dataset parameter, and only the
  // first is connected when it is added, so say which ones are missing.
  bool connected = true;
  for (auto* port : inputPorts()) {
    if (!port->link()) {
      qWarning().noquote() << QStringLiteral(
                                "%1 cannot run: input '%2' is not connected. "
                                "Drag a dataset's output port onto it.")
                                .arg(label(), port->name());
      connected = false;
    }
  }
  if (!connected) {
    setExecState(NodeExecState::Failed);
    return false;
  }
  for (auto* port : inputPorts()) {
    if (!port->hasData()) {
      setExecState(NodeExecState::Failed);
      return false;
    }
  }

  auto inputs = collectInputs();

  auto outputs = transform(inputs);

  if (isCanceled()) {
    setExecState(NodeExecState::Canceled);
    return false;
  }

  if (outputs.isEmpty() && !outputPorts().isEmpty()) {
    setExecState(NodeExecState::Failed);
    return false;
  }

  // Inherit presentation metadata (colormap, gradient opacity, …)
  // from inputs to outputs while both PortData maps are alive in
  // local scope. The dispatch is payload-type-aware and lives in
  // pipeline/PortDataMetadata; this call site stays generic.
  inheritOutputMetadata(this, inputs, outputs);

  applyOutputs(outputs);

  markCurrent();
  setExecState(NodeExecState::Idle);
  return true;
}

} // namespace pipeline
} // namespace tomviz
