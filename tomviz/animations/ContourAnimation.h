/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizContourAnimation_h
#define tomvizContourAnimation_h

#include "ModuleAnimation.h"

#include "ModuleContour.h"

namespace tomviz {

class ContourAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  double startValue = 0;
  double stopValue = 0;

  ContourAnimation(ModuleContour* module, double start, double stop)
    : ModuleAnimation(module), startValue(start), stopValue(stop)
  {
  }

  ModuleContour* module() { return qobject_cast<ModuleContour*>(baseModule); }

  void onTimeChanged() override
  {
    if (!timeKeeper()) {
      return;
    }

    // Simple interpolation
    double value = (stopValue - startValue) * progress() + startValue;
    module()->setIsoValue(value);
  }
};

} // namespace tomviz

#endif
