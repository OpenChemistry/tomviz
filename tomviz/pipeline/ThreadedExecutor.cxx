/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ThreadedExecutor.h"

#include "InputPort.h"
#include "InternalNodeExecutor.h"
#include "Link.h"
#include "Node.h"
#include "NodeExecutor.h"
#include "OutputPort.h"
#include "PassthroughOutputPort.h"
#include "Pipeline.h"
#include "PortData.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QHash>
#include <QSemaphore>
#include <QThread>

#include <memory>

namespace tomviz {
namespace pipeline {

namespace {

OutputPort* resolveSourceOutput(OutputPort* output)
{
  while (auto* pt = qobject_cast<PassthroughOutputPort*>(output)) {
    output = pt->source();
    if (!output) {
      return nullptr;
    }
  }
  return output;
}

} // namespace

class ExecutionWorker : public QObject
{
  Q_OBJECT

public:
  ExecutionWorker(std::atomic<bool>& cancelFlag,
                  std::atomic<Node*>& currentNode,
                  QObject* mainThreadContext, QSemaphore& syncSem)
    : m_cancelFlag(cancelFlag), m_currentNode(currentNode),
      m_mainThreadContext(mainThreadContext), m_syncSem(syncSem)
  {
  }

public slots:
  void run(const QList<Node*>& nodes, Pipeline* pipeline)
  {
    Q_UNUSED(pipeline);

    // Drop any stray barrier permit left by a previous run that was
    // cancelled while parked at the per-node barrier below, so this
    // run's first barrier actually waits for the main thread.
    while (m_syncSem.tryAcquire()) {
    }

    // Per-plan strong-ref retainer; see DefaultExecutor::execute for the
    // detailed rationale. Local to this slot so it drops as soon as the
    // plan finishes, evicting any transient outputs that no consumer
    // (e.g. a sink) decided to retain.
    QHash<OutputPort*, std::shared_ptr<PortData>> inflight;

    bool breakpointWasHit = false;

    for (auto* node : nodes) {
      if (m_cancelFlag.load()) {
        emit canceled();
        emit executionDone(false);
        return;
      }

      if (node->hasBreakpoint()) {
        // Skip just this node — siblings that don't depend on it can
        // still run. Downstream consumers see anyInputStale() == true
        // and skip themselves on the next iterations.
        emit breakpointHit(node);
        breakpointWasHit = true;
        continue;
      }
      if (node->isHeld()) {
        // Inserted but not applied yet: its dialog owns the first run.
        // Skipped the same way, silently.
        continue;
      }

      // No "skip Current" filter here — see DefaultExecutor for the
      // rationale. The plan is trusted to contain only nodes that need
      // to run.

      // Skip nodes whose inputs are stale due to upstream failure/cancellation
      if (node->anyInputStale()) {
        continue;
      }

      // Deliver upstream handles to this node's input ports — see
      // DefaultExecutor for the detailed rationale.
      for (auto* input : node->inputPorts()) {
        auto* link = input->link();
        if (!link || !link->from()) {
          continue;
        }
        auto* source = resolveSourceOutput(link->from());
        if (!source) {
          continue;
        }
        auto it = inflight.constFind(source);
        if (it == inflight.constEnd()) {
          if (auto h = source->take()) {
            it = inflight.insert(source, h);
          }
        }
        if (it != inflight.constEnd()) {
          input->setHandle(it.value());
        }
      }

      auto* nx = node->nodeExecutor();
      if (!nx) {
        nx = &InternalNodeExecutor::instance();
      }

      m_currentNode.store(node);
      emit nodeStarted(node);
      bool success = nx->execute(node);
      m_currentNode.store(nullptr);
      emit nodeFinished(node, success);

      // Barrier: don't start the next node until the main thread has
      // drained this node's nodeExecutionFinished handlers. The
      // color-map rescale wired to that signal pushes ParaView SM proxy
      // state through the session (vtkSMProxy::UpdateVTKObjects), which
      // is not thread-safe against the operator we'd otherwise start
      // running here concurrently — that race was the SIGBUS in
      // VolumeData::rescaleColorMap. The release is posted *after* the
      // queued nodeFinished above (same receiver, same thread → FIFO),
      // so by the time it runs the handlers have finished. The
      // cancel-checked timeout keeps ~ThreadedExecutor's QThread::wait()
      // from deadlocking against a worker parked here during teardown.
      QMetaObject::invokeMethod(
        m_mainThreadContext, [this]() { m_syncSem.release(); },
        Qt::QueuedConnection);
      while (!m_syncSem.tryAcquire(1, 50)) {
        if (m_cancelFlag.load()) {
          break;
        }
      }

      for (auto* input : node->inputPorts()) {
        input->clearHandle();
      }

      if (!success) {
        // Mark downstream nodes stale so they are skipped.
        node->markStale();
        continue;
      }

      // No eager take — publications sit on the producer's port until
      // an in-plan consumer's input-delivery loop does the lazy take.
    }

    emit executionDone(!breakpointWasHit);
  }

signals:
  void nodeStarted(Node* node);
  void nodeFinished(Node* node, bool success);
  void executionDone(bool success);
  void breakpointHit(Node* node);
  void canceled();

private:
  std::atomic<bool>& m_cancelFlag;
  std::atomic<Node*>& m_currentNode;
  QObject* m_mainThreadContext;
  QSemaphore& m_syncSem;
};

ThreadedExecutor::ThreadedExecutor(QObject* parent)
  : PipelineExecutor(parent), m_thread(new QThread(this)),
    m_worker(new ExecutionWorker(m_cancelRequested, m_currentNode, this,
                                 m_syncSem))
{
  m_worker->moveToThread(m_thread);

  // Forward worker signals to executor signals (queued connections across
  // thread boundary)
  connect(m_worker, &ExecutionWorker::nodeStarted, this,
          &PipelineExecutor::nodeExecutionStarted);
  connect(m_worker, &ExecutionWorker::nodeFinished, this,
          &PipelineExecutor::nodeExecutionFinished);
  connect(m_worker, &ExecutionWorker::canceled, this,
          &PipelineExecutor::canceled);

  connect(m_worker, &ExecutionWorker::executionDone, this,
          [this](bool success) {
            m_running = false;
            emit executionComplete(success);
            executePending();
          });

  m_thread->start();
}

ThreadedExecutor::~ThreadedExecutor()
{
  m_pendingPipeline = nullptr;
  m_pendingNodes.clear();
  cancel();
  m_thread->quit();
  // The worker may be parked in a BlockingQueuedConnection marshaling a
  // sink's consume() onto this (the GUI) thread (see SinkNode::runConsume
  // / ThreadUtils) or in the per-node barrier above. A bare wait() would
  // deadlock the first case: the worker can't return until we service the
  // posted call, but wait() doesn't run our event loop. Pump just the
  // queued meta-calls until the thread exits so any in-flight marshal
  // completes and the worker unwinds. Restricting to MetaCall avoids
  // re-dispatching paint/timer/input events mid-teardown. When the worker
  // is idle (the common case) wait(10) returns at once and nothing is
  // pumped.
  while (!m_thread->wait(10)) {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
  }
  delete m_worker;
}

void ThreadedExecutor::execute(const QList<Node*>& nodes, Pipeline* pipeline)
{
  if (m_running) {
    // Store the request and cancel; executePending() will pick it up
    // when the current run finishes.
    m_pendingNodes = nodes;
    m_pendingPipeline = pipeline;
    cancel();
    return;
  }

  // Disconnect previous breakpoint forwarding, if any
  if (m_breakpointConnection) {
    disconnect(m_breakpointConnection);
  }

  // Forward breakpointHit to the pipeline for this execution
  m_breakpointConnection = connect(
    m_worker, &ExecutionWorker::breakpointHit, pipeline,
    [pipeline](Node* node) { emit pipeline->breakpointReached(node); });

  m_cancelRequested = false;
  m_running = true;

  // Use invokeMethod to run the worker's slot on its thread
  QMetaObject::invokeMethod(
    m_worker, [this, nodes, pipeline]() { m_worker->run(nodes, pipeline); },
    Qt::QueuedConnection);
}

void ThreadedExecutor::executePending()
{
  if (m_pendingPipeline) {
    auto nodes = m_pendingNodes;
    auto* pipeline = m_pendingPipeline;
    m_pendingNodes.clear();
    m_pendingPipeline = nullptr;
    execute(nodes, pipeline);
  }
}

void ThreadedExecutor::cancel()
{
  m_cancelRequested = true;
  if (auto* node = m_currentNode.load()) {
    // cancelExecution sets the canceled flag, emits the signal, and
    // notifies the per-node executor (so an external one terminates
    // its subprocess).
    node->cancelExecution();
  }
}

void ThreadedExecutor::cancelAndWait()
{
  if (!m_running.load()) {
    return;
  }
  // Drop any queued follow-up run so executePending() can't restart us
  // out from under the wait below.
  m_pendingPipeline = nullptr;
  m_pendingNodes.clear();
  cancel();

  // Block until the worker leaves the current node and run() returns.
  // The worker only checks the cancel flag at node boundaries (a running
  // Python transform isn't interrupted mid-call), so we must wait for it
  // — Pipeline::clear() deletes nodes right after this, and the worker
  // emitting from / touching a freed Node is a SIGSEGV. executionComplete
  // fires from our executionDone handler once the worker is idle; a
  // nested event loop keeps delivering the worker's queued signals (and
  // any sink consume() marshaled to this thread) until then, so it can't
  // deadlock. ExcludeUserInputEvents avoids acting on stray clicks while
  // we're tearing down.
  QEventLoop loop;
  auto conn = connect(this, &PipelineExecutor::executionComplete, &loop,
                      &QEventLoop::quit);
  if (m_running.load()) {
    loop.exec(QEventLoop::ExcludeUserInputEvents);
  }
  disconnect(conn);
}

bool ThreadedExecutor::isRunning() const
{
  return m_running;
}

} // namespace pipeline
} // namespace tomviz

#include "ThreadedExecutor.moc"
