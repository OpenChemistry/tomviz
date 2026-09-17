/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizThresholdAnimation_h
#define tomvizThresholdAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/ThresholdSink.h"

namespace tomviz {

/// Sweeps one end of a Threshold visualization's range, so a
/// segmentation grows or shrinks over the animation while the other end
/// stays where the panel has it.
class ThresholdAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  enum End
  {
    Lower,
    Upper
  };

  double startValue = 0;
  double stopValue = 0;
  End end = Lower;

  ThresholdAnimation(pipeline::ThresholdSink* sink, double start, double stop,
                     End e)
    : ModuleAnimation(sink), startValue(start), stopValue(stop), end(e)
  {
  }

  pipeline::ThresholdSink* sink()
  {
    return qobject_cast<pipeline::ThresholdSink*>(baseNode.data());
  }

  QString type() const override { return "threshold"; }

  QString describeParameters() const override
  {
    return QString("%1 threshold %2 to %3")
      .arg(end == Lower ? "lower" : "upper")
      .arg(startValue)
      .arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    return { { "start", startValue },
             { "stop", stopValue },
             { "end", end == Lower ? "lower" : "upper" } };
  }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !sink()) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    if (end == Lower) {
      sink()->setThresholdRange(value, sink()->upperThreshold());
    } else {
      sink()->setThresholdRange(sink()->lowerThreshold(), value);
    }
  }
};

} // namespace tomviz

#endif
