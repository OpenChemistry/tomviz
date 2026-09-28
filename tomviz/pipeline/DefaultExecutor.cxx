/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DefaultExecutor.h"

#include "InputPort.h"
#include "InternalNodeExecutor.h"
#include "Link.h"
#include "Node.h"
#include "NodeExecutor.h"
#include "OutputPort.h"
#include "PassthroughOutputPort.h"
#include "Pipeline.h"
#include "PortData.h"

#include <QHash>

#include <memory>

namespace tomviz {
namespace pipeline {

namespace {

/// Walk through any chain of PassthroughOutputPorts to reach the
/// original data-owning OutputPort. SinkGroupNode's passthroughs carry
/// no data of their own — the executor's in-flight map is keyed by the
/// upstream producer, so consumers behind a passthrough need the
/// resolved source to find their handle.
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

DefaultExecutor::DefaultExecutor(QObject* parent)
  : PipelineExecutor(parent)
{}

void DefaultExecutor::execute(const QList<Node*>& nodes, Pipeline* pipeline)
{
  m_running = true;
  m_cancelRequested = false;

  // Keeps published transient outputs alive across the plan window so
  // consumers reading via inputPort->data() (or sinks stashing copies)
  // see live data. Populated lazily: an entry is created the first time
  // an in-plan consumer reads a given source. The map drops at
  // end-of-plan, so transient outputs whose handles aren't retained by
  // anyone evict automatically. Outputs without any in-plan consumer
  // are never taken from — their m_strong stays on the port, so the
  // data survives until a future plan delivers it to a consumer (e.g.
  // an executeUpstreamOf(target) run where target itself reads later).
  QHash<OutputPort*, std::shared_ptr<PortData>> inflight;

  bool breakpointHit = false;

  for (auto* node : nodes) {
    if (m_cancelRequested) {
      m_running = false;
      emit canceled();
      emit executionComplete(false);
      return;
    }

    if (node->hasBreakpoint()) {
      // Skip just this node — siblings that don't depend on it can
      // still run. Downstream consumers see anyInputStale() == true
      // and skip themselves on the next iterations.
      emit pipeline->breakpointReached(node);
      breakpointHit = true;
      continue;
    }
    if (node->isHeld()) {
      // Inserted but not applied yet: its dialog owns the first run.
      // Skipped the same way, silently.
      continue;
    }

    // No "skip Current" filter here: the plan handed to the executor is
    // already trimmed by Pipeline::executionOrder, which deliberately
    // re-includes Current nodes whose required outputs were evicted
    // (transient data dropped after the last run). Filtering at runtime
    // would silently drop those re-runs and feed downstream consumers
    // empty payloads — most visibly in SinkGroupNode chains, where the
    // passthrough output's hasData() reflects an upstream that the
    // executor just skipped.

    // Skip nodes with invalid (type-incompatible) input links
    if (node->hasInvalidInputLinks()) {
      emit nodeExecutionStarted(node);
      emit nodeExecutionFinished(node, false);
      continue;
    }

    // Skip nodes whose inputs are stale due to upstream failure/cancellation
    if (node->anyInputStale()) {
      continue;
    }

    // Deliver an upstream-payload handle to each input port before the
    // node runs. The handle is copied from the in-flight map (lazily
    // taken if the producer wasn't itself in the plan, e.g. a
    // Current+populated persistent source whose data hasn't been
    // taken yet this plan). Sinks copy this handle into their own
    // member; transforms read through it and discard.
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

    emit nodeExecutionStarted(node);
    bool success = nx->execute(node);
    emit nodeExecutionFinished(node, success);

    // Drop the delivered handles — the consumer kept its own copy if
    // it wanted retention. Leaving them on the input port would pin
    // upstream data alive for the lifetime of the InputPort and
    // defeat transient eviction.
    for (auto* input : node->inputPorts()) {
      input->clearHandle();
    }

    if (!success) {
      // Mark downstream nodes stale so they are skipped.
      node->markStale();
      continue;
    }

    // No eager take here. Publications sit on the producer's output
    // port via m_strong until an in-plan consumer's input-delivery
    // loop above does the lazy take. Outputs with no in-plan consumer
    // simply stay pinned on the port for the next plan to find.
  }

  m_running = false;
  emit executionComplete(!breakpointHit);
}

void DefaultExecutor::cancel()
{
  m_cancelRequested = true;
}

bool DefaultExecutor::isRunning() const
{
  return m_running;
}

} // namespace pipeline
} // namespace tomviz
