/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ExplodedGeometry.h"

#include <algorithm>
#include <cmath>

namespace tomviz {
namespace pipeline {

std::array<double, 3> explodedDirection(int axis,
                                        const std::array<double, 3>& custom)
{
  if (axis >= 0 && axis < 3) {
    std::array<double, 3> dir = { 0.0, 0.0, 0.0 };
    dir[axis] = 1.0;
    return dir;
  }
  const double length = std::sqrt(custom[0] * custom[0] +
                                  custom[1] * custom[1] +
                                  custom[2] * custom[2]);
  if (length <= 0.0) {
    return { 0.0, 0.0, 1.0 };
  }
  return { custom[0] / length, custom[1] / length, custom[2] / length };
}

void explodedExtent(const double bounds[6], const std::array<double, 3>& dir,
                    double& lo, double& length)
{
  // Half the projected extent is the sum of the half-widths weighted by
  // how much of each axis the direction contains; the box is symmetric
  // about its center, so lo is minus that.
  double half = 0.0;
  for (int a = 0; a < 3; ++a) {
    half += std::abs(dir[a]) * (bounds[2 * a + 1] - bounds[2 * a]) / 2.0;
  }
  lo = -half;
  length = 2.0 * half;
}

double explodedVoxelStep(const std::array<double, 3>& dir,
                         const double spacing[3])
{
  double step = 0.0;
  for (int a = 0; a < 3; ++a) {
    step += dir[a] * spacing[a] * dir[a] * spacing[a];
  }
  return std::sqrt(step);
}

int explodedOffsetLimit(double length, int chunks, double step)
{
  if (chunks < 1 || step <= 0.0) {
    return 0;
  }
  const double width = length / chunks;
  return std::max(0, static_cast<int>(std::floor((width - step) / step)));
}

double explodedShift(int voxels, double length, int chunks, double step)
{
  const int limit = explodedOffsetLimit(length, chunks, step);
  const int clamped = std::max(-limit, std::min(limit, voxels));
  return clamped * step;
}

} // namespace pipeline
} // namespace tomviz
