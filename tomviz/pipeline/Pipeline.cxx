/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Pipeline.h"

#include "DefaultExecutor.h"
#include "ExecutionFuture.h"
#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "OutputPort.h"
#include "PipelineExecutor.h"
#include "SinkGroupNode.h"
#include "SinkNode.h"
#include "TransformNode.h"

#include <QMap>
#include <QQueue>
#include <QSet>
#include <QStack>

namespace tomviz {
namespace pipeline {

Pipeline::Pipeline(QObject* parent) : QObject(parent) {}

Pipeline::~Pipeline()
{
  // Delete links before nodes to avoid dangling pointer access in Link::~Link
  qDeleteAll(m_links);
  m_links.clear();
}

void Pipeline::addNode(Node* node)
{
  if (!node || m_nodes.contains(node)) {
    return;
  }
  node->setParent(this);
  m_nodes.append(node);

  // Auto re-execute when transform parameters change
  if (auto* transform = dynamic_cast<TransformNode*>(node)) {
    connect(transform, &TransformNode::parametersApplied,
            this, [this]() { execute(); });
  }

  emit nodeAdded(node);
}

void Pipeline::removeNode(Node* node)
{
  if (!node || !m_nodes.contains(node)) {
    return;
  }

  // If this is a group node, remove all member sinks first.
  if (auto* group = qobject_cast<SinkGroupNode*>(node)) {
    const auto members = group->sinks();
    for (auto* sink : members) {
      removeNode(sink);
    }
  }

  // Remove all links connected to this node
  QList<Link*> linksToRemove;
  for (auto* link : m_links) {
    if (link->from()->node() == node || link->to()->node() == node) {
      linksToRemove.append(link);
    }
  }
  for (auto* link : linksToRemove) {
    removeLink(link);
  }

  m_nodes.removeOne(node);
  m_nodeIds.remove(node);
  emit nodeRemoved(node);
  delete node;
}

QList<Node*> Pipeline::nodes() const
{
  return m_nodes;
}

void Pipeline::clear()
{
  // Stop any in-flight execution and wait for the worker to leave the
  // current node before deleting anything. Otherwise the worker thread
  // can still be executing a node we delete below and emit from / touch
  // the freed Node — a SIGSEGV. The close-while-running path
  // (MainWindow::closeEvent -> clear()) is the one that hits this.
  if (m_executor && m_executor->isRunning()) {
    m_executor->cancelAndWait();
  }

  // Drop links first so their removal signals fire while both
  // endpoint nodes are still alive. removeNode() itself also scrubs
  // any remaining links connected to the node and deletes the node;
  // we don't touch `node` afterward.
  const auto links = m_links;
  for (auto* link : links) {
    removeLink(link);
  }
  const auto nodes = m_nodes;
  for (auto* node : nodes) {
    removeNode(node);
  }
}

QList<Node*> Pipeline::roots() const
{
  QList<Node*> result;
  for (auto* node : m_nodes) {
    if (node->inputPorts().isEmpty()) {
      result.append(node);
    } else {
      // Check if all inputs are unconnected
      bool hasConnection = false;
      for (auto* input : node->inputPorts()) {
        if (input->link()) {
          hasConnection = true;
          break;
        }
      }
      if (!hasConnection) {
        result.append(node);
      }
    }
  }
  return result;
}

int Pipeline::creationIndex(Node* node) const
{
  return m_nodes.indexOf(node);
}

int Pipeline::nodeId(Node* node)
{
  if (!node || !m_nodes.contains(node)) {
    return -1;
  }
  auto it = m_nodeIds.find(node);
  if (it != m_nodeIds.end()) {
    return it.value();
  }
  int id = m_nextNodeId++;
  m_nodeIds.insert(node, id);
  return id;
}

Node* Pipeline::nodeById(int id) const
{
  for (auto it = m_nodeIds.constBegin(); it != m_nodeIds.constEnd(); ++it) {
    if (it.value() == id) {
      return it.key();
    }
  }
  return nullptr;
}

void Pipeline::setNodeId(Node* node, int id)
{
  if (!node || !m_nodes.contains(node)) {
    return;
  }
  m_nodeIds[node] = id;
}

int Pipeline::nextNodeId() const
{
  return m_nextNodeId;
}

void Pipeline::setNextNodeId(int id)
{
  m_nextNodeId = id;
}

Link* Pipeline::createLink(OutputPort* from, InputPort* to)
{
  if (!from || !to) {
    return nullptr;
  }

  if (!to->canConnectTo(from)) {
    return nullptr;
  }

  if (!from->canAcceptLink(to)) {
    return nullptr;
  }

  if (wouldCreateCycle(from, to)) {
    return nullptr;
  }

  // Remove existing link on this input port
  if (to->link()) {
    removeLink(to->link());
  }

  auto* link = new Link(from, to, this);
  m_links.append(link);
  emit linkCreated(link);

  // An output can change type after it is linked, as a reader does when
  // it first runs, so follow it. The link is the context object, so the
  // connection goes away with the link.
  connect(from, &OutputPort::effectiveTypeChanged, link, [this, link]() {
    if (link->to()) {
      propagateEffectiveTypes(link->to()->node());
    }
  });

  // Propagate effective types and recheck link validity downstream
  propagateEffectiveTypes(to->node());

  // Mark downstream stale so it re-runs against the new input. Skip if the
  // node is still New (never configured/run) so the link-completion handler
  // in MainWindow can pop the edit dialog before first execution.
  Node* downstream = to->node();
  if (downstream && downstream->state() != NodeState::New) {
    downstream->markStale();
  }

  return link;
}

void Pipeline::removeLink(Link* link)
{
  if (!link || !m_links.contains(link)) {
    return;
  }

  // Remember the downstream node before deleting the link
  Node* downstream = link->to() ? link->to()->node() : nullptr;

  m_links.removeOne(link);
  emit linkRemoved(link);
  delete link;

  // Propagate effective types from the formerly-downstream node
  if (downstream) {
    propagateEffectiveTypes(downstream);
  }
}

QList<Link*> Pipeline::links() const
{
  return m_links;
}

bool Pipeline::wouldCreateCycle(OutputPort* from, InputPort* to) const
{
  if (!from || !to) {
    return false;
  }

  Node* sourceNode = from->node();
  Node* targetNode = to->node();

  if (!sourceNode || !targetNode) {
    return false;
  }

  if (sourceNode == targetNode) {
    return true;
  }

  // DFS from targetNode downstream to see if we can reach sourceNode.
  // If so, adding sourceNode -> targetNode would create a cycle.
  QSet<Node*> visited;
  QStack<Node*> stack;
  stack.push(targetNode);

  while (!stack.isEmpty()) {
    Node* current = stack.pop();
    if (current == sourceNode) {
      return true;
    }
    if (visited.contains(current)) {
      continue;
    }
    visited.insert(current);

    for (auto* downstream : current->downstreamNodes()) {
      if (!visited.contains(downstream)) {
        stack.push(downstream);
      }
    }
  }

  return false;
}

bool Pipeline::isValid() const
{
  // Check all links are valid
  for (auto* link : m_links) {
    if (!link->isValid()) {
      return false;
    }
  }

  // Check no cycles (try topological sort)
  // Use Kahn's algorithm - if we can sort all nodes, no cycles.
  // Count in-degree by unique upstream node, not per link.
  QMap<Node*, int> inDegree;
  for (auto* node : m_nodes) {
    inDegree[node] = 0;
  }
  for (auto* node : m_nodes) {
    for (auto* upstream : node->upstreamNodes()) {
      if (m_nodes.contains(upstream)) {
        inDegree[node]++;
      }
    }
  }

  QQueue<Node*> queue;
  for (auto it = inDegree.constBegin(); it != inDegree.constEnd(); ++it) {
    if (it.value() == 0) {
      queue.enqueue(it.key());
    }
  }

  int count = 0;
  while (!queue.isEmpty()) {
    Node* node = queue.dequeue();
    count++;
    for (auto* downstream : node->downstreamNodes()) {
      inDegree[downstream]--;
      if (inDegree[downstream] == 0) {
        queue.enqueue(downstream);
      }
    }
  }

  return count == m_nodes.size();
}

void Pipeline::depthFirstTraversal(std::function<void(Node*)> visitor,
                                   const QList<Node*>& startNodes)
{
  QList<Node*> starts = startNodes.isEmpty() ? roots() : startNodes;
  QSet<Node*> visited;
  QStack<Node*> stack;

  for (auto* node : starts) {
    stack.push(node);
  }

  while (!stack.isEmpty()) {
    Node* current = stack.pop();
    if (visited.contains(current)) {
      continue;
    }
    visited.insert(current);
    visitor(current);

    for (auto* downstream : current->downstreamNodes()) {
      if (!visited.contains(downstream)) {
        stack.push(downstream);
      }
    }
  }
}

QList<Node*> Pipeline::topologicalSort(const QList<Node*>& startNodes,
                                       SortOrder order)
{
  // Determine the subset of nodes to sort
  QSet<Node*> subset;
  if (startNodes.isEmpty()) {
    for (auto* node : m_nodes) {
      subset.insert(node);
    }
  } else {
    // Collect all nodes reachable from startNodes
    QStack<Node*> stack;
    for (auto* node : startNodes) {
      stack.push(node);
    }
    while (!stack.isEmpty()) {
      Node* current = stack.pop();
      if (subset.contains(current)) {
        continue;
      }
      subset.insert(current);
      for (auto* downstream : current->downstreamNodes()) {
        if (!subset.contains(downstream)) {
          stack.push(downstream);
        }
      }
    }
  }

  if (order == SortOrder::DepthFirst) {
    // DFS reverse post-order: keeps chains together.
    // Tiebreak among siblings/roots by creation order.
    QList<Node*> result;
    QSet<Node*> visited;

    // Build creation-index map for the subset
    QMap<Node*, int> indexMap;
    for (int i = 0; i < m_nodes.size(); ++i) {
      if (subset.contains(m_nodes[i])) {
        indexMap[m_nodes[i]] = i;
      }
    }

    auto byCreation = [&indexMap](Node* a, Node* b) {
      return indexMap.value(a, INT_MAX) < indexMap.value(b, INT_MAX);
    };

    // Collect roots within the subset, sorted by creation order
    QList<Node*> subsetRoots;
    for (auto* node : m_nodes) {
      if (!subset.contains(node)) {
        continue;
      }
      bool isRoot = true;
      for (auto* input : node->inputPorts()) {
        if (input->link() && subset.contains(input->link()->from()->node())) {
          isRoot = false;
          break;
        }
      }
      if (isRoot) {
        subsetRoots.append(node);
      }
    }
    // subsetRoots already in creation order (iterated m_nodes)

    // Iterative DFS with two-pass approach for post-order
    struct Frame
    {
      Node* node;
      bool childrenPushed;
    };
    QStack<Frame> dfsStack;
    // Push in forward creation order so that later-created roots sit on top
    // of the stack, get processed (and prepended) first, and end up after
    // earlier-created roots in the final result.
    for (int i = 0; i < subsetRoots.size(); ++i) {
      dfsStack.push({ subsetRoots[i], false });
    }

    while (!dfsStack.isEmpty()) {
      auto& frame = dfsStack.top();
      if (visited.contains(frame.node)) {
        dfsStack.pop();
        continue;
      }
      if (frame.childrenPushed) {
        // Post-order: all children processed, emit this node
        visited.insert(frame.node);
        result.prepend(frame.node);
        dfsStack.pop();
      } else {
        frame.childrenPushed = true;
        // Sort downstream by creation order, then push in forward order so
        // that later-created children sit on top, get prepended first, and
        // end up after earlier-created siblings in the final result.
        auto downstream = frame.node->downstreamNodes();
        std::sort(downstream.begin(), downstream.end(), byCreation);
        for (int i = 0; i < downstream.size(); ++i) {
          if (subset.contains(downstream[i]) &&
              !visited.contains(downstream[i])) {
            dfsStack.push({ downstream[i], false });
          }
        }
      }
    }

    return result;
  }

  // Kahn's algorithm on the subset.
  // Count in-degree by unique upstream node (not per link) so that multiple
  // links between the same pair of nodes count as a single dependency edge.
  QMap<Node*, int> inDegree;
  for (auto* node : subset) {
    inDegree[node] = 0;
  }
  for (auto* node : subset) {
    for (auto* upstream : node->upstreamNodes()) {
      if (subset.contains(upstream)) {
        inDegree[node]++;
      }
    }
  }

  if (order == SortOrder::Stable) {
    // Stable Kahn's: among nodes with in-degree 0, pick the one with the
    // smallest creation index (position in m_nodes).
    QMap<Node*, int> indexMap;
    for (int i = 0; i < m_nodes.size(); ++i) {
      if (subset.contains(m_nodes[i])) {
        indexMap[m_nodes[i]] = i;
      }
    }

    // Collect initial ready set
    QList<Node*> ready;
    for (auto it = inDegree.constBegin(); it != inDegree.constEnd(); ++it) {
      if (it.value() == 0) {
        ready.append(it.key());
      }
    }

    QList<Node*> result;
    while (!ready.isEmpty()) {
      // Pick the node with the smallest creation index
      int bestIdx = 0;
      int bestCreation = indexMap.value(ready[0], INT_MAX);
      for (int i = 1; i < ready.size(); ++i) {
        int ci = indexMap.value(ready[i], INT_MAX);
        if (ci < bestCreation) {
          bestCreation = ci;
          bestIdx = i;
        }
      }
      Node* node = ready.takeAt(bestIdx);
      result.append(node);

      for (auto* downstream : node->downstreamNodes()) {
        if (!subset.contains(downstream)) {
          continue;
        }
        inDegree[downstream]--;
        if (inDegree[downstream] == 0) {
          ready.append(downstream);
        }
      }
    }

    return result;
  }

  // Default Kahn's (original behavior)
  QQueue<Node*> queue;
  for (auto it = inDegree.constBegin(); it != inDegree.constEnd(); ++it) {
    if (it.value() == 0) {
      queue.enqueue(it.key());
    }
  }

  QList<Node*> result;
  while (!queue.isEmpty()) {
    Node* node = queue.dequeue();
    result.append(node);
    for (auto* downstream : node->downstreamNodes()) {
      if (!subset.contains(downstream)) {
        continue;
      }
      inDegree[downstream]--;
      if (inDegree[downstream] == 0) {
        queue.enqueue(downstream);
      }
    }
  }

  return result;
}

void Pipeline::setExecutor(PipelineExecutor* executor)
{
  m_executor = executor;
  if (m_executor) {
    m_executor->setParent(this);
  }
}

PipelineExecutor* Pipeline::executor() const
{
  return m_executor;
}

bool Pipeline::isExecuting() const
{
  return m_executor && m_executor->isRunning();
}

bool Pipeline::isPaused() const
{
  return m_paused;
}

void Pipeline::setPaused(bool paused)
{
  if (m_paused == paused) {
    return;
  }
  m_paused = paused;
  emit pausedChanged(m_paused);
  if (!m_paused) {
    // Resume: execute if any node is stale.
    for (auto* node : m_nodes) {
      if (node->state() == NodeState::Stale ||
          node->state() == NodeState::New) {
        execute();
        break;
      }
    }
  }
}

void Pipeline::cancelExecution()
{
  if (m_executor && m_executor->isRunning()) {
    m_executor->cancel();
  }
}

ExecutionFuture* Pipeline::execute()
{
  // Compute the plan by treating each leaf (no downstream consumers)
  // as a target and merging the per-target execution orders. This
  // routes through executionOrder()'s logic, so a Current node whose
  // required output has been evicted (transient data dropped after the
  // last run) is re-included only when a downstream consumer in the
  // plan actually needs that output. Without this, the executor's
  // simple "skip Current" filter would let stale-looking-but-evicted
  // intermediates feed an empty payload into downstream sinks.
  QList<Node*> order;
  QSet<Node*> inOrder;
  for (auto* node : m_nodes) {
    if (!node->downstreamNodes().isEmpty()) {
      continue;
    }
    auto sub = executionOrder(node);
    for (auto* n : sub) {
      if (!inOrder.contains(n)) {
        inOrder.insert(n);
        order.append(n);
      }
    }
  }
  // executionOrder() returns each per-leaf plan topo-sorted internally,
  // but merging two such lists may leave the result out of order when
  // a node appears in both. Re-sort the union to guarantee global
  // topological order.
  if (!order.isEmpty()) {
    QMap<Node*, int> inDegree;
    for (auto* n : order) {
      inDegree[n] = 0;
    }
    for (auto* n : order) {
      for (auto* up : n->upstreamNodes()) {
        if (inOrder.contains(up)) {
          inDegree[n]++;
        }
      }
    }
    QQueue<Node*> ready;
    for (auto it = inDegree.constBegin(); it != inDegree.constEnd(); ++it) {
      if (it.value() == 0) {
        ready.enqueue(it.key());
      }
    }
    QList<Node*> sorted;
    while (!ready.isEmpty()) {
      Node* n = ready.dequeue();
      sorted.append(n);
      for (auto* d : n->downstreamNodes()) {
        if (!inOrder.contains(d)) {
          continue;
        }
        if (--inDegree[d] == 0) {
          ready.enqueue(d);
        }
      }
    }
    order = sorted;
  }

  return runPlan(order);
}

ExecutionFuture* Pipeline::execute(Node* target)
{
  return runPlan(executionOrder(target));
}

void Pipeline::executeWhenIdle()
{
  if (!isExecuting()) {
    execute();
    return;
  }
  if (!m_idleExecuteQueued) {
    m_idleExecuteQueued = true;
    connect(this, &Pipeline::executionFinished, this,
            [this]() {
              m_idleExecuteQueued = false;
              execute();
            },
            static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));
  }
}

ExecutionFuture* Pipeline::executeUpstreamOf(Node* target)
{
  return runPlan(upstreamExecutionOrder(target));
}

ExecutionFuture* Pipeline::runPlan(QList<Node*> order)
{
  if (m_paused) {
    auto* future = new ExecutionFuture(this);
    future->setFinished(false);
    return future;
  }

  if (!m_executor) {
    auto* defaultExec = new DefaultExecutor(this);
    setExecutor(defaultExec);
  }

  if (order.isEmpty()) {
    auto* future = new ExecutionFuture(this);
    future->setFinished(true);
    // Emit executionFinished even for empty plans so callers waiting on
    // the signal (e.g. the properties panel's not-ready warning) refresh
    // their state. No executionStarted is paired with it — nothing
    // actually ran, so toggling mutation-locked UI on/off would just
    // flicker. Consumers must therefore tolerate a Finished without a
    // matching prior Started.
    emit executionFinished();
    return future;
  }

  if (m_executor->isRunning()) {
    m_executor->cancel();
  }

  auto* future = new ExecutionFuture(this);

  connect(m_executor, &PipelineExecutor::executionComplete, future,
          [this, future](bool success) {
            future->setFinished(success);
            emit executionFinished();
          },
          static_cast<Qt::ConnectionType>(Qt::AutoConnection |
                                          Qt::SingleShotConnection));

  m_executor->execute(order, this);
  emit executionStarted();

  return future;
}

namespace {

// Topo-sort + dependency walk shared by executionOrder() and
// upstreamExecutionOrder(). The caller seeds @a stack with the nodes
// that should be considered the "leaves" of the plan; this walks each
// node's input ports backward, pulling in any upstream node whose
// feeding output is missing or whose state isn't Current. The result
// is the topo-sorted union, excluding any node in @a exclude.
QList<Node*> buildExecutionPlan(QStack<Node*> stack,
                                const QSet<Node*>& exclude = {})
{
  QSet<Node*> needed;

  while (!stack.isEmpty()) {
    Node* current = stack.pop();
    if (needed.contains(current) || exclude.contains(current)) {
      continue;
    }
    needed.insert(current);

    // Step to each upstream producer by walking the specific link feeding
    // this input. A Current upstream is re-included only when the output
    // we actually depend on has been evicted — other evicted outputs on
    // the same node don't matter.
    for (auto* input : current->inputPorts()) {
      auto* link = input->link();
      if (!link || !link->from()) {
        continue;
      }
      auto* upstreamOutput = link->from();
      auto* upstream = upstreamOutput->node();
      if (!upstream || needed.contains(upstream) ||
          exclude.contains(upstream)) {
        continue;
      }
      bool upstreamNeedsRerun =
        upstream->state() != NodeState::Current ||
        !upstreamOutput->hasData();
      if (upstreamNeedsRerun) {
        stack.push(upstream);
      }
    }
  }

  if (needed.isEmpty()) {
    return {};
  }

  // Topological sort of just the needed nodes.
  // Count in-degree by unique upstream node, not per link.
  QMap<Node*, int> inDegree;
  for (auto* node : needed) {
    inDegree[node] = 0;
  }
  for (auto* node : needed) {
    for (auto* upstream : node->upstreamNodes()) {
      if (needed.contains(upstream)) {
        inDegree[node]++;
      }
    }
  }

  QQueue<Node*> queue;
  for (auto it = inDegree.constBegin(); it != inDegree.constEnd(); ++it) {
    if (it.value() == 0) {
      queue.enqueue(it.key());
    }
  }

  QList<Node*> result;
  while (!queue.isEmpty()) {
    Node* node = queue.dequeue();
    result.append(node);
    for (auto* downstream : node->downstreamNodes()) {
      if (!needed.contains(downstream)) {
        continue;
      }
      inDegree[downstream]--;
      if (inDegree[downstream] == 0) {
        queue.enqueue(downstream);
      }
    }
  }

  return result;
}

} // namespace

QList<Node*> Pipeline::executionOrder(Node* target)
{
  if (!target) {
    return {};
  }

  // Whether target itself needs to (re-)run. Current targets with all
  // outputs still materialized have nothing to do; Current targets
  // whose outputs have been evicted (transient data dropped after the
  // last execution) must re-run to repopulate.
  auto needsRerun = [](Node* node) {
    if (node->state() != NodeState::Current) {
      return true;
    }
    for (auto* output : node->outputPorts()) {
      if (!output->hasData()) {
        return true;
      }
    }
    return false;
  };

  QStack<Node*> stack;
  if (needsRerun(target)) {
    stack.push(target);
  }
  return buildExecutionPlan(std::move(stack));
}

QList<Node*> Pipeline::upstreamExecutionOrder(Node* target)
{
  if (!target) {
    return {};
  }

  // Seed with each upstream node feeding a missing/stale input. Same
  // per-link gate as executionOrder(): a Current upstream is only
  // re-included when the specific output we depend on has been evicted.
  // Target itself is excluded from the result even if it would otherwise
  // be reached via the walk (it can't be, since we only walk upstream).
  QStack<Node*> stack;
  for (auto* input : target->inputPorts()) {
    auto* link = input->link();
    if (!link || !link->from()) {
      continue;
    }
    auto* upstreamOutput = link->from();
    auto* upstream = upstreamOutput->node();
    if (!upstream) {
      continue;
    }
    bool upstreamNeedsRerun =
      upstream->state() != NodeState::Current ||
      !upstreamOutput->hasData();
    if (upstreamNeedsRerun) {
      stack.push(upstream);
    }
  }
  return buildExecutionPlan(std::move(stack), { target });
}

void Pipeline::propagateEffectiveTypes(Node* startNode)
{
  if (!startNode) {
    return;
  }

  // Forward BFS from startNode through the DAG
  QQueue<Node*> queue;
  QSet<Node*> visited;
  queue.enqueue(startNode);

  while (!queue.isEmpty()) {
    Node* node = queue.dequeue();
    if (visited.contains(node)) {
      continue;
    }
    visited.insert(node);

    node->recomputeEffectiveTypes();

    // Recheck validity of all links from this node's outputs
    for (auto* output : node->outputPorts()) {
      for (auto* link : output->links()) {
        link->recheck();
        // Continue propagation to downstream nodes
        if (link->to() && link->to()->node()) {
          Node* downstream = link->to()->node();
          if (!visited.contains(downstream)) {
            queue.enqueue(downstream);
          }
        }
      }
    }
  }
}

} // namespace pipeline
} // namespace tomviz
