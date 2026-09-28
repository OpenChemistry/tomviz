/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTimeSeriesAnimation_h
#define tomvizTimeSeriesAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"

namespace tomviz {

/// Plays a time series loaded on a source node: on every animation tick
/// it switches the VolumeData to the step nearest the current time and
/// re-notifies the port's consumers so directly-linked sinks re-consume
/// the new image. Created by LoadDataReaction when a time series is
/// loaded; like every ModuleAnimation it deletes itself when its node
/// goes away. Gated at tick time on the global enable flag, which the
/// Animation Helper's "time series" checkbox toggles.
class TimeSeriesAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  TimeSeriesAnimation(pipeline::SourceNode* source) : ModuleAnimation(source)
  {
  }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !baseNode) {
      return;
    }
    if (!activeObjects().timeSeriesAnimationsEnabled()) {
      return;
    }
    // The worker thread may be reading this VolumeData mid-execution;
    // don't swap images out from under it.
    auto* pip = activeObjects().pipeline();
    if (pip && pip->isExecuting()) {
      return;
    }

    auto* port = volumePort();
    if (!port) {
      return;
    }
    auto vol = port->data().value<pipeline::VolumeDataPtr>();
    if (!vol) {
      return;
    }

    int numSteps = vol->timeSteps().size();
    if (numSteps <= 1) {
      return;
    }

    int index = qRound(progress() * (numSteps - 1));
    index = qBound(0, index, numSteps - 1);
    if (index == vol->currentTimeStepIndex()) {
      return;
    }

    vol->switchTimeStep(index);
    port->notifyDataMutated();
  }

private:
  // The output port on the source carrying a time-series VolumeData.
  pipeline::OutputPort* volumePort()
  {
    for (auto* port : baseNode->outputPorts()) {
      if (!port->hasData()) {
        continue;
      }
      // value() falls back to an empty pointer when the port carries
      // something else, so a non-volume port is skipped, not thrown.
      auto vol = port->data().value<pipeline::VolumeDataPtr>();
      if (vol && vol->hasTimeSteps()) {
        return port;
      }
    }
    return nullptr;
  }
};

} // namespace tomviz

#endif
