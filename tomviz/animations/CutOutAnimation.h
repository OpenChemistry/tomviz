/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCutOutAnimation_h
#define tomvizCutOutAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/VolumeSink.h"

namespace tomviz {

/// Slides the cut-out of a volume along one axis, as a fraction of the
/// volume, so the interior is revealed progressively. Switches the
/// cut-out on if it is off, since the sweep would otherwise show nothing.
class CutOutAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  double startValue = 0;
  double stopValue = 0;
  int axis = 0;

  CutOutAnimation(pipeline::VolumeSink* sink, double start, double stop,
                  int a)
    : ModuleAnimation(sink), startValue(start), stopValue(stop),
      axis(qBound(0, a, 2))
  {
  }

  pipeline::VolumeSink* sink()
  {
    return qobject_cast<pipeline::VolumeSink*>(baseNode.data());
  }

  QString type() const override { return "cutOut"; }

  QString describeParameters() const override
  {
    return QString("cut-out %1 %2 to %3")
      .arg(QChar('X' + axis))
      .arg(startValue)
      .arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    return { { "start", startValue }, { "stop", stopValue }, { "axis", axis } };
  }

  void onPlaybackStarted() override { m_switchedOn = false; }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !sink()) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    sink()->setCutOutPosition(axis, value);
    // Once per playback, so a volume that cannot be cut (rendered in
    // bricks) does not warn on every tick
    if (!m_switchedOn && !sink()->cutOutEnabled()) {
      sink()->setCutOutEnabled(true);
    }
    m_switchedOn = true;
  }

private:
  bool m_switchedOn = false;
};

} // namespace tomviz

#endif
