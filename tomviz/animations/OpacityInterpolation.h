/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOpacityInterpolation_h
#define tomvizOpacityInterpolation_h

class vtkPiecewiseFunction;

namespace tomviz {

/// Blend two scalar opacity curves into @a out, which may alias either
/// input. @a u runs 0 (all @a from) to 1 (all @a to) and is clamped.
///
/// Both curves are read over @a range, normally
/// VolumeData::colorMapRange(): they are stored in data coordinates and
/// move when the data range does, so two curves only agree once read
/// over the same window.
void interpolateOpacity(vtkPiecewiseFunction* from, vtkPiecewiseFunction* to,
                        double u, const double range[2],
                        vtkPiecewiseFunction* out);

} // namespace tomviz

#endif
