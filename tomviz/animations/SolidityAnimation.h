/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSolidityAnimation_h
#define tomvizSolidityAnimation_h

#include "ModuleAnimation.h"
#include "pipeline/sinks/VolumeSink.h"

#include <cmath>

namespace tomviz {

/// A volume's solidity swept from one value to another. Solidity scales
/// how much each unit of depth absorbs, so the sweep is geometric: equal
/// steps of time multiply it by equal factors, which reads as an even
/// thickening rather than one that is all over in the first moments.
class SolidityAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  double startValue = 1;
  double stopValue = 1;

  SolidityAnimation(pipeline::VolumeSink* sink, double start, double stop)
    : ModuleAnimation(sink), startValue(start), stopValue(stop)
  {
  }

  /// The value @a u (0 to 1) of the way from @a start to @a stop
  static double blend(double start, double stop, double u)
  {
    if (start <= 0 || stop <= 0) {
      return start + (stop - start) * u;
    }
    return std::exp(std::log(start) + (std::log(stop) - std::log(start)) * u);
  }

  pipeline::VolumeSink* sink()
  {
    return qobject_cast<pipeline::VolumeSink*>(baseNode.data());
  }

  QString type() const override { return "solidity"; }
  QString describeParameters() const override
  {
    return QString("solidity %1 to %2").arg(startValue).arg(stopValue);
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
    sink()->setSolidity(blend(startValue, stopValue, progress()));
  }
};

} // namespace tomviz

#endif
