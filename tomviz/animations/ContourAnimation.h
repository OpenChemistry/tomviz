/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizContourAnimation_h
#define tomvizContourAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/ContourSink.h"

namespace tomviz {

class ContourAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  double startValue = 0;
  double stopValue = 0;

  ContourAnimation(pipeline::ContourSink* sink, double start, double stop)
    : ModuleAnimation(sink), startValue(start), stopValue(stop)
  {
  }

  pipeline::ContourSink* sink()
  {
    return qobject_cast<pipeline::ContourSink*>(baseNode.data());
  }

  QString type() const override { return "contour"; }

  QString describeParameters() const override
  {
    return QString("iso value %1 to %2").arg(startValue).arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    return { { "start", startValue }, { "stop", stopValue } };
  }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !sink()) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    sink()->setIsoValue(value);
  }
};

} // namespace tomviz

#endif
