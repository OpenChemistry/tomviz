/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizClipAnimation_h
#define tomvizClipAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/ClipSink.h"

namespace tomviz {

/// Sweeps a clipping plane between two positions along its own normal.
/// An axis-aligned clip moves by slice index, the unit its own property
/// panel uses; a custom-oriented clip has no slices, so it moves by
/// signed distance from the center of the data instead.
class ClipAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  enum Unit
  {
    Slice,
    Distance
  };

  double startValue = 0;
  double stopValue = 0;
  Unit unit = Slice;

  ClipAnimation(pipeline::ClipSink* sink, double start, double stop, Unit u)
    : ModuleAnimation(sink), startValue(start), stopValue(stop), unit(u)
  {
  }

  pipeline::ClipSink* sink()
  {
    return qobject_cast<pipeline::ClipSink*>(baseNode.data());
  }

  QString type() const override { return "clip"; }

  QString describeParameters() const override
  {
    return QString("%1 %2 to %3")
      .arg(unit == Slice ? "slice" : "position")
      .arg(startValue)
      .arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    // The unit is saved rather than re-derived on load: a clip whose
    // direction changed between sessions would otherwise come back
    // reading slice indices as distances, or the other way around.
    return { { "start", startValue },
             { "stop", stopValue },
             { "unit", unit == Slice ? "slice" : "distance" } };
  }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !sink()) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    if (unit == Slice) {
      sink()->setSlice(qRound(value));
    } else {
      sink()->setPlaneDistance(value);
    }
  }
};

} // namespace tomviz

#endif
