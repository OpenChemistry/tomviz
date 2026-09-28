/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineAutoExecuteController_h
#define tomvizPipelineAutoExecuteController_h

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

class QThread;

namespace tomviz {
namespace pipeline {

class Node;
class Pipeline;

/// Runs one should_auto_execute() query. Lives on the controller's
/// worker thread so the query — which runs user Python, or spawns an
/// external subprocess — never blocks the GUI. One instance serves all
/// nodes; queued invocations serialize on its event loop, so at most
/// one query runs at a time.
class AutoExecuteWorker : public QObject
{
  Q_OBJECT

public slots:
  void runQuery(tomviz::pipeline::Node* node);

signals:
  void queryFinished(tomviz::pipeline::Node* node, bool shouldExecute);
};

/// Drives the per-node "periodic execution" option: keeps a timer per
/// node that has it enabled; a firing timer queues the node for a
/// should_auto_execute() query. Queries run one at a time on a worker
/// thread, and only while the pipeline is idle and unpaused — timer
/// fires that land while the pipeline is busy collapse into a single
/// pending entry per node, so a slow pipeline sees at most one
/// follow-up check (and hence at most one re-execution) no matter how
/// many intervals elapsed. A query answering true marks the node stale
/// and schedules a pipeline execution.
class AutoExecuteController : public QObject
{
  Q_OBJECT

public:
  explicit AutoExecuteController(Pipeline* pipeline,
                                 QObject* parent = nullptr);
  ~AutoExecuteController() override;

signals:
  /// Internal: hands one query to the worker thread.
  void queryRequested(tomviz::pipeline::Node* node);

private:
  void onNodeAdded(Node* node);
  void onNodeRemoved(Node* node);
  /// Create/retune or tear down the node's timer to match its
  /// auto-execute setting.
  void syncNode(Node* node);
  void onQueryFinished(Node* node, bool shouldExecute);
  /// Dispatch the next pending query, if the pipeline is idle and no
  /// query is in flight.
  void drain();

  Pipeline* m_pipeline;
  /// One timer per auto-execute-enabled node. Key presence doubles as
  /// "node is alive and has the feature on" for the guards.
  QHash<Node*, QTimer*> m_timers;
  QSet<Node*> m_pending;
  Node* m_inFlight = nullptr;
  QThread* m_thread = nullptr;
  AutoExecuteWorker* m_worker = nullptr;
  /// Backstop re-drain while the pipeline is busy: a run queued behind
  /// a cancelled one finishes without emitting executionFinished (see
  /// Pipeline::runPlan's single-shot wiring), so waiting on the signal
  /// alone could strand pending checks.
  QTimer m_retryTimer;
};

} // namespace pipeline
} // namespace tomviz

#endif
