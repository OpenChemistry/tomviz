/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPlaneIndexing_h
#define tomvizPlaneIndexing_h

#include <QtGlobal>

namespace tomviz {
namespace pipeline {

/// Conversions between a slice index along an axis and the physical
/// coordinate of that slice, shared by the slice and clip sinks. dims
/// and bounds are the image's extents and physical bounds.
namespace planeindex {

inline double spacing(const int dims[3], const double bounds[6], int axis)
{
  if (axis < 0 || axis > 2 || dims[axis] < 2) {
    return 0.0;
  }
  return (bounds[2 * axis + 1] - bounds[2 * axis]) / (dims[axis] - 1);
}

/// Physical coordinate of slice `index`; 0 when the geometry is unknown.
inline double position(const int dims[3], const double bounds[6], int axis,
                       int index)
{
  return bounds[2 * axis] + index * spacing(dims, bounds, axis);
}

/// Nearest slice index to a physical coordinate, clamped to the extents;
/// -1 when the geometry is unknown.
inline int index(const int dims[3], const double bounds[6], int axis,
                 double pos)
{
  double sp = spacing(dims, bounds, axis);
  if (sp <= 0.0) {
    return -1;
  }
  return qBound(0, qRound((pos - bounds[2 * axis]) / sp), dims[axis] - 1);
}

} // namespace planeindex
} // namespace pipeline
} // namespace tomviz

#endif
