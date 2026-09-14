/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineLabelMapSurface_h
#define tomvizPipelineLabelMapSurface_h

#include <vtkSmartPointer.h>

#include <QVector>

class vtkImageData;
class vtkPolyData;

namespace tomviz {
namespace pipeline {

class LabelTable;

/// The surface representation of a label map: one closed surface per
/// label, extracted with Surface Nets. Unlike an isosurface, which
/// presumes a continuous field, Surface Nets handles piecewise-constant
/// data directly, keeps adjacent labels watertight against each other,
/// and smooths under constraints so regions do not shrink.
///
/// Free functions rather than a sink so they can be tested headless.

/// The labels drawn as regions: every table entry whose value is not the
/// background. Hidden entries are included, so hiding a label does not
/// change how its neighbours are smoothed; see extractLabelSurface.
QVector<double> regionLabels(const LabelTable& table, double background);

/// The subset of regionLabels() the user has left visible.
QVector<double> visibleLabels(const LabelTable& table, double background);

/// Extract the faces bounding @a visible, treating every label outside
/// @a regions as @a background. Faces between a visible region and a
/// hidden one are kept, so a hidden neighbour reads as a cavity. With
/// @a smoothingIterations above zero the mesh is relaxed and given point
/// normals; at zero the voxel faces are returned as-is, and the mapper's
/// per-face normals give them their crisp look. Returns an empty polydata
/// when there is nothing to draw or the image is not a 3D volume.
vtkSmartPointer<vtkPolyData> extractLabelSurface(
  vtkImageData* image, const QVector<double>& regions,
  const QVector<double>& visible, int smoothingIterations,
  double background);

/// The name of the RGB cell array colorLabelSurface writes.
extern const char* const kLabelColorsArrayName;

/// Color each face by the visible label it bounds, from the table's
/// colors, into a cell array named kLabelColorsArrayName. Cheap, so a
/// color edit does not need a new extraction. Faces whose labels are
/// not in the table are grey.
void colorLabelSurface(vtkPolyData* surface, const LabelTable& table,
                       double background);

} // namespace pipeline
} // namespace tomviz

#endif
