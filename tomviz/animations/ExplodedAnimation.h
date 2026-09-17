/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizExplodedAnimation_h
#define tomvizExplodedAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/VolumeSink.h"

namespace tomviz {

/// Sweeps one setting of a volume's exploded view: the gap between the
/// slabs, or how many slabs the volume is cut into. Switches the exploded
/// view on if it is off, since the sweep would otherwise show nothing.
class ExplodedAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  enum Unit
  {
    Gap,
    Chunks
  };

  double startValue = 0;
  double stopValue = 0;
  Unit unit = Gap;

  ExplodedAnimation(pipeline::VolumeSink* sink, double start, double stop,
                    Unit u)
    : ModuleAnimation(sink), startValue(start), stopValue(stop), unit(u)
  {
  }

  pipeline::VolumeSink* sink()
  {
    return qobject_cast<pipeline::VolumeSink*>(baseNode.data());
  }

  QString type() const override { return "exploded"; }

  QString describeParameters() const override
  {
    return QString("%1 %2 to %3")
      .arg(unit == Gap ? "exploded gap" : "exploded chunks")
      .arg(startValue)
      .arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    return { { "start", startValue },
             { "stop", stopValue },
             { "unit", unit == Gap ? "gap" : "chunks" } };
  }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !sink()) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    if (unit == Gap) {
      sink()->setExplodedGap(value);
    } else {
      sink()->setExplodedChunks(qRound(value));
    }
    // A no-op once it is on, so the camera is refit at most once.
    if (!sink()->explodedEnabled()) {
      sink()->setExplodedEnabled(true);
    }
  }
};

} // namespace tomviz

#endif
