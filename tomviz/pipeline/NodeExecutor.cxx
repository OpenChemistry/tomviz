/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodeExecutor.h"

#include "Node.h"

namespace tomviz {
namespace pipeline {

NodeExecutor::NodeExecutor(QObject* parent) : QObject(parent) {}

void NodeExecutor::cancel(Node* /*node*/)
{
  // Default no-op. Node::cancelExecution has already set the canceled
  // flag and emitted the signal before invoking us; in-process
  // executors don't need anything more.
}

void NodeExecutor::complete(Node* /*node*/)
{
  // Default no-op. Same shape as cancel(): the completed flag is
  // already set by Node::completeExecution; in-process executors are
  // satisfied with that.
}

bool NodeExecutor::shouldAutoExecute(Node* node)
{
  // Default: evaluate in-process. Out-of-process executors override to
  // ask their external environment instead.
  return node && node->queryShouldAutoExecute();
}

QJsonObject NodeExecutor::serialize() const
{
  return {};
}

bool NodeExecutor::deserialize(const QJsonObject&)
{
  return true;
}

} // namespace pipeline
} // namespace tomviz
