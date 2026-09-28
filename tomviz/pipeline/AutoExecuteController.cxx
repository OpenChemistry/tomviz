/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AutoExecuteController.h"

#include "InternalNodeExecutor.h"
#include "Node.h"
#include "NodeExecutor.h"
#include "Pipeline.h"

#include <QThread>

namespace tomviz {
namespace pipeline {

void AutoExecuteWorker::runQuery(Node* node)
{
  auto* executor = node ? node->nodeExecutor() : nullptr;
  if (!executor) {
    executor = &InternalNodeExecutor::instance();
  }
  emit queryFinished(node, executor->shouldAutoExecute(node));
}

AutoExecuteController::AutoExecuteController(Pipeline* pipeline,
                                             QObject* parent)
  : QObject(parent), m_pipeline(pipeline)
{
  // The worker connections are queued (cross-thread); Node* must be a
  // registered metatype under the spelling moc records for the
  // signals.
  qRegisterMetaType<Node*>("Node*");
  qRegisterMetaType<Node*>("tomviz::pipeline::Node*");

  m_thread = new QThread(this);
  m_thread->setObjectName(QStringLiteral("AutoExecuteQuery"));
  auto* worker = new AutoExecuteWorker;
  worker->moveToThread(m_thread);
  m_worker = worker;
  connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(this, &AutoExecuteController::queryRequested, worker,
          &AutoExecuteWorker::runQuery);
  connect(worker, &AutoExecuteWorker::queryFinished, this,
          &AutoExecuteController::onQueryFinished);
  m_thread->start();

  m_retryTimer.setSingleShot(true);
  m_retryTimer.setInterval(2000);
  connect(&m_retryTimer, &QTimer::timeout, this,
          &AutoExecuteController::drain);

  connect(m_pipeline, &Pipeline::nodeAdded, this,
          &AutoExecuteController::onNodeAdded);
  connect(m_pipeline, &Pipeline::nodeRemoved, this,
          &AutoExecuteController::onNodeRemoved);
  connect(m_pipeline, &Pipeline::executionFinished, this,
          &AutoExecuteController::drain);
  connect(m_pipeline, &Pipeline::pausedChanged, this,
          [this](bool) { drain(); });

  for (auto* node : m_pipeline->nodes()) {
    onNodeAdded(node);
  }
}

AutoExecuteController::~AutoExecuteController()
{
  // An in-flight query keeps running until it completes (an external
  // check is bounded by its kill timeout); its result is dropped.
  m_thread->quit();
  m_thread->wait();
}

void AutoExecuteController::onNodeAdded(Node* node)
{
  // Connected for every node, enabled or not: the setting can be
  // turned on later (editor Apply, state load applying deserialize
  // after addNode).
  connect(node, &Node::autoExecuteChanged, this,
          [this, node]() { syncNode(node); });
  syncNode(node);
}

void AutoExecuteController::onNodeRemoved(Node* node)
{
  disconnect(node, nullptr, this, nullptr);
  if (auto* timer = m_timers.take(node)) {
    timer->deleteLater();
  }
  m_pending.remove(node);
  if (node == m_inFlight) {
    // Let the worker finish with the node before it is deleted
    QMetaObject::invokeMethod(m_worker, [] {}, Qt::BlockingQueuedConnection);
  }
}

void AutoExecuteController::syncNode(Node* node)
{
  if (node->autoExecuteEnabled()) {
    QTimer* timer = m_timers.value(node);
    if (!timer) {
      timer = new QTimer(this);
      connect(timer, &QTimer::timeout, this, [this, node]() {
        // Collapse: a node already pending stays a single entry no
        // matter how many intervals elapse while the pipeline is busy.
        m_pending.insert(node);
        drain();
      });
      m_timers.insert(node, timer);
    }
    int intervalMs = node->autoExecuteIntervalSeconds() * 1000;
    if (timer->interval() != intervalMs || !timer->isActive()) {
      timer->start(intervalMs);
    }
  } else {
    if (auto* timer = m_timers.take(node)) {
      timer->deleteLater();
    }
    m_pending.remove(node);
  }
}

void AutoExecuteController::onQueryFinished(Node* node, bool shouldExecute)
{
  if (node == m_inFlight) {
    m_inFlight = nullptr;
  }
  // The m_timers guard covers both "node was removed while the query
  // ran" (the pointer may be dangling — compare only, never
  // dereference) and "auto-execute was turned off meanwhile".
  if (shouldExecute && m_timers.contains(node)) {
    node->markStale();
    m_pipeline->executeWhenIdle();
  }
  drain();
}

void AutoExecuteController::drain()
{
  if (m_inFlight || m_pending.isEmpty()) {
    return;
  }
  if (m_pipeline->isExecuting() || m_pipeline->isPaused()) {
    // executionFinished / pausedChanged re-drain; the retry timer is
    // the backstop for completions that emit no signal.
    m_retryTimer.start();
    return;
  }
  while (!m_pending.isEmpty()) {
    Node* node = *m_pending.begin();
    m_pending.remove(node);
    if (!m_timers.contains(node)) {
      continue;
    }
    m_inFlight = node;
    emit queryRequested(node);
    return;
  }
}

} // namespace pipeline
} // namespace tomviz
