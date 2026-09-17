/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineExplodedGeometry_h
#define tomvizPipelineExplodedGeometry_h

#include <array>

namespace tomviz {
namespace pipeline {

/// The axis value that means "use the custom direction" in the volume
/// sink's exploded view.
constexpr int kExplodedCustomAxis = 3;

/// The unit direction the slabs are cut and pulled apart along: the axis
/// for 0-2, otherwise @a custom normalized. A zero custom vector falls
/// back to +Z rather than divide by nothing.
std::array<double, 3> explodedDirection(int axis,
                                        const std::array<double, 3>& custom);

/// How far the box @a bounds reaches along @a dir, measured from its
/// centre: @a lo is the signed distance of the nearest corner (never
/// positive) and @a length the full span, so the slab boundaries lie at
/// lo + length * k / chunks. For an axis direction this is the box's own
/// extent along that axis.
void explodedExtent(const double bounds[6], const std::array<double, 3>& dir,
                    double& lo, double& length);

} // namespace pipeline
} // namespace tomviz

#endif
