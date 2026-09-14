/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LabelMapSurface.h"

#include "data/LabelMapData.h"

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSurfaceNets3D.h>
#include <vtkUnsignedCharArray.h>

#include <QColor>

#include <algorithm>
#include <array>
#include <unordered_map>

namespace tomviz {
namespace pipeline {

const char* const kLabelColorsArrayName = "LabelColors";

QVector<double> regionLabels(const LabelTable& table, double background)
{
  QVector<double> labels;
  for (const auto& entry : table.entries()) {
    if (entry.value != background) {
      labels.append(entry.value);
    }
  }
  return labels;
}

QVector<double> visibleLabels(const LabelTable& table, double background)
{
  QVector<double> labels;
  for (const auto& entry : table.entries()) {
    if (entry.visible && entry.value != background) {
      labels.append(entry.value);
    }
  }
  return labels;
}

vtkSmartPointer<vtkPolyData> extractLabelSurface(
  vtkImageData* image, const QVector<double>& regions,
  const QVector<double>& visible, int smoothingIterations, double background)
{
  auto surface = vtkSmartPointer<vtkPolyData>::New();
  if (!image || regions.isEmpty() || visible.isEmpty()) {
    return surface;
  }
  int dims[3];
  image->GetDimensions(dims);
  if (dims[0] < 2 || dims[1] < 2 || dims[2] < 2) {
    // Surface Nets is a 3D algorithm; a single slice has no closed
    // surface to extract.
    return surface;
  }
  auto* scalars = image->GetPointData()->GetScalars();
  if (!scalars || scalars->GetNumberOfComponents() != 1) {
    return surface;
  }

  // The shallow copy shares the voxel arrays but owns its attribute
  // bookkeeping, so the active array cannot change under the filter
  // while another sink reading the same image picks a different one.
  vtkNew<vtkImageData> input;
  input->ShallowCopy(image);

  vtkNew<vtkSurfaceNets3D> nets;
  nets->SetInputData(input);
  nets->SetBackgroundLabel(background);
  nets->SetNumberOfLabels(regions.size());
  for (int i = 0; i < regions.size(); ++i) {
    nets->SetLabel(i, regions[i]);
  }
  // Every region takes part in the extraction and the smoothing, and
  // only the visible ones are output, so toggling a label's checkbox
  // does not reshape its neighbours.
  nets->SetOutputStyleToSelected();
  nets->InitializeSelectedLabelsList();
  for (double label : visible) {
    if (regions.contains(label)) {
      nets->AddSelectedLabel(label);
    }
  }
  if (nets->GetNumberOfSelectedLabels() == 0) {
    return surface;
  }
  nets->SetSmoothing(smoothingIterations > 0);
  nets->SetNumberOfIterations(std::max(0, smoothingIterations));
  nets->Update();

  if (smoothingIterations > 0) {
    // A relaxed mesh reads best with smooth (point) normals. The mesh
    // is non-manifold wherever three labels meet, so consistency is
    // enforced without traversing those edges, where it could loop.
    vtkNew<vtkPolyDataNormals> normals;
    normals->SetInputConnection(nets->GetOutputPort());
    normals->SplittingOff();
    normals->ConsistencyOn();
    normals->NonManifoldTraversalOff();
    normals->ComputePointNormalsOn();
    normals->ComputeCellNormalsOff();
    normals->Update();
    surface->ShallowCopy(normals->GetOutput());
  } else {
    surface->ShallowCopy(nets->GetOutput());
  }
  return surface;
}

void colorLabelSurface(vtkPolyData* surface, const LabelTable& table,
                       double background)
{
  if (!surface) {
    return;
  }
  const vtkIdType count = surface->GetNumberOfCells();
  vtkNew<vtkUnsignedCharArray> colors;
  colors->SetName(kLabelColorsArrayName);
  colors->SetNumberOfComponents(3);
  colors->SetNumberOfTuples(count);

  std::unordered_map<double, std::array<unsigned char, 3>> palette;
  for (const auto& entry : table.entries()) {
    if (entry.visible && entry.value != background) {
      palette[entry.value] = { static_cast<unsigned char>(entry.color.red()),
                               static_cast<unsigned char>(entry.color.green()),
                               static_cast<unsigned char>(entry.color.blue()) };
    }
  }

  // Surface Nets tags each face with the labels on its two sides. A face
  // bounding a visible region is colored by that region; a face between
  // two visible regions is only seen through a translucent surface, and
  // takes the first side's color.
  auto* boundary = surface->GetCellData()->GetArray("BoundaryLabels");
  const bool tagged = boundary && boundary->GetNumberOfComponents() == 2 &&
                      boundary->GetNumberOfTuples() == count;
  const std::array<unsigned char, 3> grey = { 128, 128, 128 };
  for (vtkIdType c = 0; c < count; ++c) {
    const std::array<unsigned char, 3>* rgb = &grey;
    if (tagged) {
      for (int side = 0; side < 2 && rgb == &grey; ++side) {
        auto it = palette.find(boundary->GetComponent(c, side));
        if (it != palette.end()) {
          rgb = &it->second;
        }
      }
    }
    colors->SetTypedTuple(c, rgb->data());
  }

  surface->GetCellData()->AddArray(colors);
  surface->GetCellData()->SetActiveScalars(kLabelColorsArrayName);
}

} // namespace pipeline
} // namespace tomviz
