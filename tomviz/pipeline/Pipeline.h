/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePipeline_h
#define tomvizPipelinePipeline_h

#include <QHash>
#include <QList>
#include <QObject>

#include <functional>

namespace tomviz {
namespace pipeline {

class ExecutionFuture;
class InputPort;
class Link;
class Node;
class OutputPort;
class PipelineExecutor;

enum class SortOrder
{
  Default,    // Kahn's algorithm, arbitrary tiebreaking (by pointer address)
  Stable,     // Kahn's algorithm, creation-order tiebreaking
  DepthFirst  // DFS reverse post-order (keeps chains together)
};

class Pipeline : public QObject
{
  Q_OBJECT

public:
  Pipeline(QObject* parent = nullptr);
  ~Pipeline() override;

  // Node management
  void addNode(Node* node);
  void removeNode(Node* node);
  QList<Node*> nodes() const;
  QList<Node*> roots() const;

  /// Remove every node and link. Used by the file-reset action and by
  /// state-file loaders that replace the whole graph.
  void clear();

  /// Return the index at which @a node was added (creation order).
  /// Returns -1 if the node is not in this pipeline.
  int creationIndex(Node* node) const;

  /// File-scoped integer id for @a node, used by the state-file schema
  /// as a stable cross-reference within a single saved session. Assigns
  /// a fresh id on first access (monotonic, never reused). Returns -1
  /// for null or unowned nodes.
  int nodeId(Node* node);

  /// Reverse lookup: the node with the given id, or nullptr.
  Node* nodeById(int id) const;

  /// Explicitly set the id for @a node. Used by the state loader to
  /// preserve ids persisted in the file so re-saves round-trip
  /// identically.
  void setNodeId(Node* node, int id);

  /// Next id the allocator will hand out. Written to the state file so
  /// re-saves don't collide with ids for nodes added after load.
  int nextNodeId() const;
  void setNextNodeId(int id);

  // Link management
  Link* createLink(OutputPort* from, InputPort* to);
  void removeLink(Link* link);
  QList<Link*> links() const;

  // Validation
  bool wouldCreateCycle(OutputPort* from, InputPort* to) const;
  bool isValid() const;

  // Traversal
  void depthFirstTraversal(std::function<void(Node*)> visitor,
                           const QList<Node*>& startNodes = {});
  QList<Node*> topologicalSort(
    const QList<Node*>& startNodes = {},
    SortOrder order = SortOrder::Default);

  // Execution
  void setExecutor(PipelineExecutor* executor);
  PipelineExecutor* executor() const;
  bool isExecuting() const;
  bool isPaused() const;
  void setPaused(bool paused);
  ExecutionFuture* execute();
  ExecutionFuture* execute(Node* target);

  /// Schedule execution after the current run finishes, without cancelling
  /// it. Used when adding sinks to avoid re-running upstream operators.
  /// With @a target, run only what it needs, not every stale node.
  void executeWhenIdle(Node* target = nullptr);

  /// Run the smallest subset of the pipeline needed to make every input
  /// port of @a target carry current data — without re-running target
  /// itself. Used by the properties-panel/edit-dialog "Run Pipeline to
  /// Generate Inputs" action when a node's custom widget needs live
  /// input data to render.
  ExecutionFuture* executeUpstreamOf(Node* target);

  void cancelExecution();
  QList<Node*> executionOrder(Node* target);

  /// Plan-only counterpart to executeUpstreamOf(): topo-sorted node
  /// list that excludes @a target.
  QList<Node*> upstreamExecutionOrder(Node* target);

  /// Recompute effective types starting from @a startNode and propagating
  /// downstream.  Rechecks link validity for all affected links.
  void propagateEffectiveTypes(Node* startNode);

signals:
  void nodeAdded(Node* node);
  void nodeRemoved(Node* node);
  void linkCreated(Link* link);
  void linkRemoved(Link* link);
  void executionStarted();
  void executionFinished();
  void pausedChanged(bool paused);
  void breakpointReached(Node* node);

private:
  ExecutionFuture* runPlan(QList<Node*> order);

  QList<Node*> m_nodes;
  QList<Link*> m_links;
  QHash<Node*, int> m_nodeIds;
  int m_nextNodeId = 1;
  PipelineExecutor* m_executor = nullptr;
  bool m_paused = false;
  bool m_idleExecuteQueued = false;
  QList<Node*> m_idleTargets;
};

} // namespace pipeline
} // namespace tomviz

#endif
