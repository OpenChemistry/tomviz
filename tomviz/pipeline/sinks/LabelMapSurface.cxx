/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LabelMapSurface.h"

#include "data/LabelMapData.h"

#include <vtkArrayDispatch.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataArrayRange.h>
#include <vtkIdList.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSurfaceNets3D.h>
#include <vtkUnsignedCharArray.h>
#include <vtkWindowedSincPolyDataFilter.h>

#include <QColor>

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tomviz {
namespace pipeline {

namespace {

// The windowed sinc pass band: lower is smoother. At this value the
// staircase is gone by 16 iterations and a sphere a few voxels across
// keeps its volume to within a percent or two; lower values start to
// shrink small regions again past 20 iterations.
constexpr double kSurfaceSmoothingPassBand = 0.05;

/// Pull every relaxed point back to within @a limit of where the voxel
/// faces put it. The smoother is unconstrained, so this is what keeps
/// the surface on the voxel shell however many iterations run.
struct HoldNearVoxelFaces
{
  double limit;

  // Sequential on purpose: vtkSMPTools drags in TBB, whose emit() does
  // not survive Qt's emit macro, and this pass is a small fraction of
  // the smoothing it follows.
  template <typename OriginalArray, typename RelaxedArray>
  void operator()(OriginalArray* original, RelaxedArray* relaxed) const
  {
    const auto from = vtk::DataArrayTupleRange<3>(original);
    auto to = vtk::DataArrayTupleRange<3>(relaxed);
    auto f = from.begin();
    for (auto t = to.begin(); t != to.end(); ++t, ++f) {
      double d[3];
      for (int k = 0; k < 3; ++k) {
        d[k] = static_cast<double>((*t)[k]) - static_cast<double>((*f)[k]);
      }
      const double length = vtkMath::Norm(d);
      if (length > limit) {
        const double scale = limit / length;
        for (int k = 0; k < 3; ++k) {
          (*t)[k] = static_cast<double>((*f)[k]) + d[k] * scale;
        }
      }
    }
  }
};

void holdNearVoxelFaces(vtkPoints* original, vtkPoints* relaxed,
                        const double spacing[3])
{
  if (!original || !relaxed ||
      original->GetNumberOfPoints() != relaxed->GetNumberOfPoints()) {
    return;
  }
  HoldNearVoxelFaces worker{ 0.5 * vtkMath::Norm(spacing) };
  using Dispatcher =
    vtkArrayDispatch::Dispatch2ByValueType<vtkArrayDispatch::Reals,
                                           vtkArrayDispatch::Reals>;
  if (!Dispatcher::Execute(original->GetData(), relaxed->GetData(), worker)) {
    worker(original->GetData(), relaxed->GetData());
  }
  relaxed->Modified();
}

} // namespace

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

vtkSmartPointer<vtkPolyData> extractLabelMesh(vtkImageData* image,
                                              const QVector<double>& regions,
                                              int smoothingIterations,
                                              double background)
{
  auto surface = vtkSmartPointer<vtkPolyData>::New();
  if (!image || regions.isEmpty()) {
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
  // Every region takes part in the extraction, and every face comes
  // out; selectLabelFaces picks the visible ones, so toggling a label's
  // checkbox does not reshape its neighbours or cost another pass over
  // the volume.
  nets->SetOutputStyleToDefault();
  // Surface Nets' own smoother is a constrained Laplacian, and it may
  // move a point a whole voxel diagonal: it rounds a convex region
  // inward, and a particle five voxels across came out at 40% of its
  // volume. The relaxation is done below instead, shrink-free.
  nets->SetSmoothing(false);
  const bool smooth = smoothingIterations > 0;
  if (smooth) {
    // Relaxed quads are no longer planar; triangles shade consistently.
    nets->SetOutputMeshTypeToTriangles();
  }
  nets->Update();
  if (!smooth) {
    surface->ShallowCopy(nets->GetOutput());
    return surface;
  }

  vtkNew<vtkWindowedSincPolyDataFilter> smoother;
  smoother->SetInputConnection(nets->GetOutputPort());
  smoother->SetNumberOfIterations(
    std::min(smoothingIterations, kMaxSurfaceSmoothing));
  smoother->SetPassBand(kSurfaceSmoothingPassBand);
  // Three labels meeting make a non-manifold edge. Left alone, the
  // points on it stay pinned and every contact between particles keeps
  // its staircase.
  smoother->NonManifoldSmoothingOn();
  smoother->FeatureEdgeSmoothingOff();
  smoother->NormalizeCoordinatesOn();
  smoother->Update();

  // The relaxation is unconstrained; this is the constraint. Held to
  // half a voxel diagonal, the surface cannot leave the voxel shell the
  // volume representation draws.
  auto* relaxed = smoother->GetOutput();
  double spacing[3];
  image->GetSpacing(spacing);
  holdNearVoxelFaces(nets->GetOutput()->GetPoints(), relaxed->GetPoints(),
                     spacing);

  // A relaxed mesh reads best with smooth (point) normals. The mesh
  // is non-manifold wherever three labels meet, so consistency is
  // enforced without traversing those edges, where it could loop.
  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputData(relaxed);
  normals->SplittingOff();
  normals->ConsistencyOn();
  normals->NonManifoldTraversalOff();
  normals->ComputePointNormalsOn();
  normals->ComputeCellNormalsOff();
  normals->Update();
  surface->ShallowCopy(normals->GetOutput());
  return surface;
}

vtkSmartPointer<vtkPolyData> selectLabelFaces(vtkPolyData* mesh,
                                              const QVector<double>& visible)
{
  auto surface = vtkSmartPointer<vtkPolyData>::New();
  if (!mesh || mesh->GetNumberOfCells() == 0 || visible.isEmpty()) {
    return surface;
  }
  const vtkIdType count = mesh->GetNumberOfCells();
  auto* boundary = mesh->GetCellData()->GetArray("BoundaryLabels");
  if (!boundary || boundary->GetNumberOfComponents() != 2 ||
      boundary->GetNumberOfTuples() != count) {
    // Not a Surface Nets mesh; nothing to select by.
    surface->ShallowCopy(mesh);
    return surface;
  }

  const std::unordered_set<double> shown(visible.begin(), visible.end());
  std::vector<vtkIdType> kept;
  kept.reserve(count);
  for (vtkIdType c = 0; c < count; ++c) {
    if (shown.count(boundary->GetComponent(c, 0)) ||
        shown.count(boundary->GetComponent(c, 1))) {
      kept.push_back(c);
    }
  }
  if (kept.empty()) {
    return surface;
  }
  if (static_cast<vtkIdType>(kept.size()) == count) {
    surface->ShallowCopy(mesh);
    return surface;
  }

  // Surface Nets writes polygons only, so a cell id is an index into
  // the polys, and the points (with their normals) can be shared as
  // they are: unreferenced points cost the mapper nothing to skip. The
  // cells go through the public cell API only; the storage-specific
  // accessors changed meaning in VTK 9.6 and read garbage there.
  surface->SetPoints(mesh->GetPoints());
  surface->GetPointData()->PassData(mesh->GetPointData());
  auto* inPolys = mesh->GetPolys();
  const vtkIdType keptCount = static_cast<vtkIdType>(kept.size());
  vtkNew<vtkCellArray> polys;
  polys->AllocateExact(keptCount, keptCount * inPolys->GetMaxCellSize());
  auto* cellData = surface->GetCellData();
  cellData->CopyAllocate(mesh->GetCellData(), keptCount);
  vtkNew<vtkIdList> pts;
  vtkIdType next = 0;
  for (vtkIdType c : kept) {
    inPolys->GetCellAtId(c, pts);
    polys->InsertNextCell(pts);
    cellData->CopyData(mesh->GetCellData(), c, next++);
  }
  surface->SetPolys(polys);
  return surface;
}

vtkSmartPointer<vtkPolyData> extractLabelSurface(
  vtkImageData* image, const QVector<double>& regions,
  const QVector<double>& visible, int smoothingIterations, double background)
{
  auto mesh =
    extractLabelMesh(image, regions, smoothingIterations, background);
  return selectLabelFaces(mesh, visible);
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
