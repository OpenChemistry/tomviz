/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSliceAnimation_h
#define tomvizSliceAnimation_h

#include "ModuleAnimation.h"

#include "ModuleSlice.h"

namespace tomviz {

class SliceAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  double startValue = 0;
  double stopValue = 0;

  SliceAnimation(ModuleSlice* module, double start, double stop)
    : ModuleAnimation(module), startValue(start), stopValue(stop)
  {
  }

  ModuleSlice* module() { return qobject_cast<ModuleSlice*>(baseModule); }

  void onTimeChanged() override
  {
    if (!timeKeeper()) {
      return;
    }

    // Simple interpolation
    double value = (stopValue - startValue) * progress() + startValue;
    module()->onSliceChanged(value);
  }
};

} // namespace tomviz

#endif
