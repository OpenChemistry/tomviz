/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TransformUtils.h"

#include "ActiveObjects.h"
#include "MainWindow.h"

#include "pipeline/DeferredLinkInfo.h"
#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/Node.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/SinkNode.h"
#include "pipeline/NodeEditDialog.h"
#include "pipeline/TransformNode.h"

#include <QApplication>
#include <QSet>
#include <QtDebug>

namespace tomviz {

/// True for SinkNode and SinkGroupNode (terminal pipeline nodes).
static bool isTerminalNode(pipeline::Node* node)
{
  return dynamic_cast<pipeline::SinkNode*>(node) ||
         dynamic_cast<pipeline::SinkGroupNode*>(node);
}

/// First output port of @a transform whose type is compatible with @a
/// sinkInput's accepted types, or nullptr if none exists.
static pipeline::OutputPort* findCompatibleOutputPort(
  pipeline::TransformNode* transform, pipeline::InputPort* sinkInput)
{
  for (auto* out : transform->outputPorts()) {
    if (pipeline::isPortTypeCompatible(out->type(),
                                       sinkInput->acceptedTypes())) {
      return out;
    }
  }
  return nullptr;
}

/// All nodes reachable downstream from @a start (inclusive), following output
/// links.  This is exactly the set Node::markStale() cascades over when @a
/// start is marked stale, so it is the set whose state we must snapshot to be
/// able to undo an eager insertion.
static QSet<pipeline::Node*> downstreamClosure(pipeline::Node* start)
{
  QSet<pipeline::Node*> result;
  if (!start) {
    return result;
  }
  QList<pipeline::Node*> stack;
  stack.append(start);
  while (!stack.isEmpty()) {
    auto* node = stack.takeLast();
    if (result.contains(node)) {
      continue;
    }
    result.insert(node);
    for (auto* out : node->outputPorts()) {
      for (auto* link : out->links()) {
        if (link->to() && link->to()->node()) {
          stack.append(link->to()->node());
        }
      }
    }
  }
  return result;
}

/// Snapshot the node and output-port states of @a nodes into @a deferred so
/// they can be restored verbatim if an eager insertion is canceled.  Must be
/// called before the insertion mutates the pipeline.
static void captureStates(const QSet<pipeline::Node*>& nodes,
                          pipeline::DeferredLinkInfo& deferred)
{
  for (auto* node : nodes) {
    deferred.nodeStates.append({ node, node->state() });
    for (auto* out : node->outputPorts()) {
      deferred.portStaleStates.append({ out, out->isStale() });
    }
  }
}

/// Append a transform at the given targetPort, moving sink/group links to a
/// compatible output of the new transform.  Sinks with no compatible output
/// on the new transform are left connected to @a targetPort.
static void appendTransformAtPort(
  pipeline::Pipeline* pip,
  pipeline::TransformNode* transform,
  pipeline::OutputPort* targetPort)
{
  pip->addNode(transform);
  pip->createLink(targetPort, transform->inputPorts()[0]);

  struct Move
  {
    pipeline::Link* link;
    pipeline::OutputPort* newOut;
  };
  QList<Move> moves;
  for (auto* link : targetPort->links()) {
    if (link->to()->node() == transform) {
      continue; // skip the link we just created
    }
    if (!isTerminalNode(link->to()->node())) {
      continue;
    }
    auto* newOut = findCompatibleOutputPort(transform, link->to());
    if (!newOut) {
      continue;
    }
    moves.append({ link, newOut });
  }
  for (const auto& m : moves) {
    auto* sinkInput = m.link->to();
    pip->removeLink(m.link);
    pip->createLink(m.newOut, sinkInput);
  }
}

/// Deferred variant of appendTransformAtPort: performs the full append
/// eagerly (so the preview shows the final topology) and returns the info
/// needed to undo it if the user cancels.
static pipeline::DeferredLinkInfo appendTransformAtPortDeferred(
  pipeline::Pipeline* pip,
  pipeline::TransformNode* transform,
  pipeline::OutputPort* targetPort)
{
  // Snapshot the downstream subtree before mutating anything so a cancel can
  // restore it exactly (the sink moves below mark it stale).
  pipeline::DeferredLinkInfo deferred;
  captureStates(downstreamClosure(targetPort->node()), deferred);

  pip->addNode(transform);
  pip->createLink(targetPort, transform->inputPorts()[0]);

  // Move terminal (sink) links from targetPort onto the new transform's
  // compatible outputs now, instead of deferring it to commit, so the preview
  // shows the sinks already routed through the transform.  Record each
  // original link so it can be recreated on cancel.
  struct Move
  {
    pipeline::Link* link;
    pipeline::OutputPort* newOut;
  };
  QList<Move> moves;
  for (auto* link : targetPort->links()) {
    if (link->to()->node() == transform) {
      continue; // skip the link we just created
    }
    if (!isTerminalNode(link->to()->node())) {
      continue;
    }
    auto* newOut = findCompatibleOutputPort(transform, link->to());
    if (!newOut) {
      continue;
    }
    moves.append({ link, newOut });
  }
  for (const auto& m : moves) {
    auto* sinkInput = m.link->to();
    deferred.linksToRestore.append({ targetPort, sinkInput });
    pip->removeLink(m.link);
    pip->createLink(m.newOut, sinkInput);
    // removeLink() reset this terminal sink/group's visualization. Re-show it
    // (the VTK objects still hold the last data) so the downstream module
    // stays on screen while the dialog is open. Nothing clears it again until
    // the not-yet-run transform produces output and the sink swaps it in
    // directly, so it also stays visible all through execution. Mirrors the
    // restorePresentation() the dialog's cancel path performs.
    if (auto* node = sinkInput->node()) {
      node->restorePresentation();
    }
  }
  return deferred;
}

/// Show a NodeEditDialog for a newly inserted transform with deferred
/// link info.
static void showInsertionDialog(
  pipeline::TransformNode* transform,
  pipeline::Pipeline* pip,
  const pipeline::DeferredLinkInfo& deferred,
  QWidget* parent)
{
  auto* dialog = new pipeline::NodeEditDialog(
    transform, pip, deferred, parent);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(
    QString("Configure - %1").arg(transform->label()));
  dialog->show();
}

/// Insert a transform at a link: break the existing link, connect the
/// link's "from" port to the new transform's input, and connect the new
/// transform's output to the link's "to" port.
static void insertTransformAtLink(
  pipeline::Pipeline* pip,
  pipeline::TransformNode* transform,
  pipeline::Link* link)
{
  auto* fromPort = link->from();
  auto* toPort = link->to();
  pip->removeLink(link);
  pip->addNode(transform);
  pip->createLink(fromPort, transform->inputPorts()[0]);
  pip->createLink(transform->outputPorts()[0], toPort);
}

/// Deferred variant of insertTransformAtLink: performs the full insertion
/// eagerly (so the preview shows the transform in its final, inserted
/// position) and returns the info needed to undo it if the user cancels.
static pipeline::DeferredLinkInfo insertTransformAtLinkDeferred(
  pipeline::Pipeline* pip,
  pipeline::TransformNode* transform,
  pipeline::Link* link)
{
  auto* fromPort = link->from();
  auto* toPort = link->to();

  // Snapshot the downstream subtree before mutating anything so a cancel can
  // restore it exactly (createLink() below marks this subtree stale).
  pipeline::DeferredLinkInfo deferred;
  captureStates(downstreamClosure(toPort->node()), deferred);
  deferred.linksToRestore.append({ fromPort, toPort });

  // Perform the full insertion now (break from->to, splice the transform in)
  // rather than only connecting the input and deferring the rest to commit.
  pip->removeLink(link);
  pip->addNode(transform);
  pip->createLink(fromPort, transform->inputPorts()[0]);
  pip->createLink(transform->outputPorts()[0], toPort);

  return deferred;
}

bool insertTransformIntoPipeline(pipeline::TransformNode* transform)
{
  auto* mainWindow = MainWindow::instance();
  auto* pip = mainWindow ? mainWindow->pipeline() : nullptr;
  if (!pip) {
    qCritical("insertTransformIntoPipeline: No active pipeline. "
              "Load data first. (mainWindow=%p)",
              static_cast<void*>(mainWindow));
    delete transform;
    return false;
  }

  // Ctrl held: add the node unconnected (user will link manually)
  if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
    pip->addNode(transform);
    return true;
  }

  auto& ao = ActiveObjects::instance();
  auto* input = transform->inputPorts()[0];

  // If a link is selected and its "to" is a transform, insert between them
  auto* activeLink = ao.activeLink();
  if (activeLink &&
      dynamic_cast<pipeline::TransformNode*>(activeLink->to()->node())) {
    auto* fromPort = activeLink->from();
    if (!pipeline::isPortTypeCompatible(fromPort->type(),
                                        input->acceptedTypes())) {
      qCritical("Incompatible port types: transform input does not accept "
                "the link's output port type.");
      delete transform;
      return false;
    }
    // Multi-input: only the first input gets connected here.  Commit
    // immediately and wait for the user to connect remaining inputs via
    // manual linking (which triggers the MainWindow linkRequested handler).
    if (transform->inputPorts().size() > 1) {
      insertTransformAtLink(pip, transform, activeLink);
    } else if (transform->hasPropertiesWidget()) {
      auto deferred =
        insertTransformAtLinkDeferred(pip, transform, activeLink);
      showInsertionDialog(transform, pip, deferred, mainWindow);
    } else {
      insertTransformAtLink(pip, transform, activeLink);
      pip->execute();
    }
    return true;
  }

  // Otherwise append at the tip output port
  auto* tipPort = ao.activeTipOutputPort();
  if (!tipPort) {
    qCritical("insertTransformIntoPipeline: No output port available. "
              "Load data first.");
    delete transform;
    return false;
  }

  if (!pipeline::isPortTypeCompatible(tipPort->type(),
                                      input->acceptedTypes())) {
    qCritical("Incompatible port types: transform input does not accept "
              "the tip output port type.");
    delete transform;
    return false;
  }

  // Multi-input: same as above — commit and wait for remaining connections.
  if (transform->inputPorts().size() > 1) {
    appendTransformAtPort(pip, transform, tipPort);
  } else if (transform->hasPropertiesWidget()) {
    auto deferred = appendTransformAtPortDeferred(pip, transform, tipPort);
    showInsertionDialog(transform, pip, deferred, mainWindow);
  } else {
    appendTransformAtPort(pip, transform, tipPort);
    pip->execute();
  }
  return true;
}

} // namespace tomviz
