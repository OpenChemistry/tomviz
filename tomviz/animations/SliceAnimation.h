/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSliceAnimation_h
#define tomvizSliceAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/SliceSink.h"

namespace tomviz {

/// Sweeps a slice through the data. An axis-aligned slice moves by
/// slice index, the unit its own property panel uses; a custom-oriented
/// slice has no slices, so it moves by signed distance from the centre
/// of the data along its own normal instead.
class SliceAnimation : public ModuleAnimation
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

  SliceAnimation(pipeline::SliceSink* sink, double start, double stop,
                 Unit u = Slice)
    : ModuleAnimation(sink), startValue(start), stopValue(stop), unit(u)
  {
  }

  pipeline::SliceSink* sink()
  {
    return qobject_cast<pipeline::SliceSink*>(baseNode.data());
  }

  QString type() const override { return "slice"; }

  QString describeParameters() const override
  {
    return QString("%1 %2 to %3")
      .arg(unit == Slice ? "slice" : "position")
      .arg(startValue)
      .arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    // The unit is saved rather than re-derived on load: a slice whose
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
      sink()->setSlice(static_cast<int>(value));
    } else {
      sink()->setPlaneDistance(value);
    }
  }
};

} // namespace tomviz

#endif
