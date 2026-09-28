/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizExplodedAnimation_h
#define tomvizExplodedAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/VolumeSink.h"

namespace tomviz {

/// Sweeps one setting of a volume's exploded view: the gap between the
/// slabs, how many slabs the volume is cut into, or the offset of the
/// cuts. Switches the exploded view on if it is off, since the sweep
/// would otherwise show nothing.
class ExplodedAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  enum Unit
  {
    Gap,
    Chunks,
    Offset
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
      .arg(unit == Gap      ? "exploded gap"
           : unit == Chunks ? "exploded chunks"
                            : "exploded offset")
      .arg(startValue)
      .arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    return { { "start", startValue },
             { "stop", stopValue },
             { "unit", unit == Gap      ? "gap"
                       : unit == Chunks ? "chunks"
                                        : "offset" } };
  }

  void onPlaybackStarted() override { m_switchedOn = false; }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !sink()) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    if (unit == Gap) {
      sink()->setExplodedGap(value);
    } else if (unit == Chunks) {
      sink()->setExplodedChunks(qRound(value));
    } else {
      sink()->setExplodedOffset(qRound(value));
    }
    // Once per playback: a volume that cannot be exploded (rendered in
    // bricks) refuses with a warning, which need not repeat every tick.
    // No camera refit, since a camera path may be flying.
    if (!m_switchedOn && !sink()->explodedEnabled()) {
      sink()->setExplodedEnabled(true, /*refitCamera=*/false);
    }
    m_switchedOn = true;
  }

private:
  bool m_switchedOn = false;
};

} // namespace tomviz

#endif
