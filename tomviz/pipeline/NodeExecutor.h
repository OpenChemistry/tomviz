/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeExecutor_h
#define tomvizPipelineNodeExecutor_h

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace tomviz {
namespace pipeline {

class Node;

/// Strategy for running a single Node. Pipeline-level executors
/// (DefaultExecutor, ThreadedExecutor) walk the graph; a NodeExecutor
/// decides where/how a single node actually runs (in-process,
/// subprocess in a foreign Python env, container, ...).
///
/// Progress and lifecycle are surfaced through the Node's existing
/// signals — implementations should drive setProgressStep / setProgress
/// Message / setTotalProgressSteps on @a node so the UI plumbing does
/// not need to know which executor is in use.
class NodeExecutor : public QObject
{
  Q_OBJECT

public:
  NodeExecutor(QObject* parent = nullptr);
  ~NodeExecutor() override = default;

  /// Run @a node. Blocks until done. Returns true on success.
  virtual bool execute(Node* node) = 0;

  /// Executor-specific cancellation hook, invoked by
  /// Node::cancelExecution after the canceled flag has already been
  /// set. The default is a no-op — in-process executors rely solely on
  /// the flag, which polling transform code observes at its next
  /// checkpoint. Subclasses that drive out-of-process work override
  /// this to forward the request across the boundary.
  virtual void cancel(Node* node);

  /// Executor-specific early-completion hook, invoked by
  /// Node::completeExecution after the completed flag has already
  /// been set. Same semantics as cancel(): in-process executors get
  /// nothing extra; out-of-process executors override to forward the
  /// signal to their child.
  virtual void complete(Node* node);

  /// Ask whether @a node wants a periodic execution now (the
  /// auto-execute poll). The hook runs in the same environment
  /// execute() would use, so a node whose script only imports in its
  /// external env still answers correctly. Blocks until done — call
  /// from a worker thread. The default runs in-process via
  /// Node::queryShouldAutoExecute().
  virtual bool shouldAutoExecute(Node* node);

  /// Identifier written to the node's serialized "executor" block.
  /// Used by NodeExecutorFactory to round-trip the executor on load.
  /// Returning an empty string suppresses serialization (e.g. the
  /// internal executor — its absence is the default).
  virtual QString type() const = 0;

  /// Subclass-specific configuration round-tripped under the
  /// "executor" block alongside `type`. Default is empty.
  virtual QJsonObject serialize() const;
  virtual bool deserialize(const QJsonObject& json);
};

} // namespace pipeline
} // namespace tomviz

#endif
