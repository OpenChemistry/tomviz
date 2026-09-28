/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "pipeline/data/LabelMapData.h"
#include "pipeline/sinks/LabelMapSurface.h"

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkMassProperties.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>
#include <vtkUnsignedCharArray.h>

#include <QColor>

#include <cmath>
#include <set>
#include <tuple>

using namespace tomviz::pipeline;

namespace {

// Three labelled blocks in a 12x12x12 volume: 1 and 2 share a face, 3
// is isolated. Background is 0.
vtkSmartPointer<vtkImageData> threeLabels()
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(12, 12, 12);
  image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  for (int z = 0; z < 12; ++z) {
    for (int y = 0; y < 12; ++y) {
      for (int x = 0; x < 12; ++x) {
        unsigned char v = 0;
        if (z >= 2 && z < 6 && y >= 2 && y < 6 && x >= 2 && x < 6) {
          v = 1;
        } else if (z >= 2 && z < 6 && y >= 2 && y < 6 && x >= 6 && x < 10) {
          v = 2;
        } else if (z >= 8 && z < 11 && y >= 8 && y < 11 && x >= 8 && x < 11) {
          v = 3;
        }
        *static_cast<unsigned char*>(image->GetScalarPointer(x, y, z)) = v;
      }
    }
  }
  return image;
}

LabelTable tableFor()
{
  LabelTable table;
  QVector<QPair<double, vtkIdType>> scanned = { { 0.0, 1 }, { 1.0, 1 },
                                                 { 2.0, 1 }, { 3.0, 1 } };
  table.reconcile(scanned, false);
  table.setColor(table.indexOfValue(1.0), QColor(255, 0, 0));
  table.setColor(table.indexOfValue(2.0), QColor(0, 255, 0));
  table.setColor(table.indexOfValue(3.0), QColor(0, 0, 255));
  return table;
}

std::set<std::tuple<int, int, int>> distinctColors(vtkPolyData* surface)
{
  std::set<std::tuple<int, int, int>> colors;
  auto* arr = vtkUnsignedCharArray::SafeDownCast(
    surface->GetCellData()->GetArray(kLabelColorsArrayName));
  if (!arr) {
    return colors;
  }
  for (vtkIdType c = 0; c < arr->GetNumberOfTuples(); ++c) {
    unsigned char rgb[3];
    arr->GetTypedTuple(c, rgb);
    colors.insert({ rgb[0], rgb[1], rgb[2] });
  }
  return colors;
}

void checkColoring(int smoothing)
{
  auto image = threeLabels();
  LabelTable table = tableFor();
  const double background = 0.0;
  auto surface =
    extractLabelSurface(image, regionLabels(table, background),
                        visibleLabels(table, background), smoothing, background);
  ASSERT_GT(surface->GetNumberOfCells(), 0);
  auto* boundary = surface->GetCellData()->GetArray("BoundaryLabels");
  ASSERT_NE(boundary, nullptr) << "smoothing=" << smoothing;
  EXPECT_EQ(boundary->GetNumberOfComponents(), 2);
  EXPECT_EQ(boundary->GetNumberOfTuples(), surface->GetNumberOfCells());

  colorLabelSurface(surface, table, background);
  auto colors = distinctColors(surface);
  EXPECT_EQ(colors.size(), 3u) << "smoothing=" << smoothing;
  EXPECT_TRUE(colors.count({ 255, 0, 0 }));
  EXPECT_TRUE(colors.count({ 0, 255, 0 }));
  EXPECT_TRUE(colors.count({ 0, 0, 255 }));
  EXPECT_FALSE(colors.count({ 128, 128, 128 })) << "grey fallback used";
}

} // namespace

TEST(LabelMapSurfaceTest, ColorsEachLabelUnsmoothed)
{
  checkColoring(0);
}

TEST(LabelMapSurfaceTest, ColorsEachLabelSmoothed)
{
  checkColoring(20);
}

namespace {

// A radius-4 sphere of label 1 centered in a 16^3 volume: 257 voxels, a
// particle of the size a segmentation is full of.
vtkSmartPointer<vtkImageData> smallSphere(const double spacing[3])
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(16, 16, 16);
  image->SetSpacing(spacing);
  image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  for (int z = 0; z < 16; ++z) {
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        const int dx = x - 8, dy = y - 8, dz = z - 8;
        *static_cast<unsigned char*>(image->GetScalarPointer(x, y, z)) =
          dx * dx + dy * dy + dz * dz <= 16 ? 1 : 0;
      }
    }
  }
  return image;
}

double enclosedVolume(vtkPolyData* surface)
{
  vtkNew<vtkTriangleFilter> triangles;
  triangles->SetInputData(surface);
  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputConnection(triangles->GetOutputPort());
  normals->ConsistencyOn();
  normals->AutoOrientNormalsOn();
  normals->SplittingOff();
  vtkNew<vtkMassProperties> mass;
  mass->SetInputConnection(normals->GetOutputPort());
  mass->Update();
  return mass->GetVolume();
}

} // namespace

// Surface Nets' own smoother rounded a particle this size down to 58% of
// its voxels; the surface has to keep the volume the voxels have.
TEST(LabelMapSurfaceTest, SmoothingKeepsTheVolume)
{
  const double unit[3] = { 1.0, 1.0, 1.0 };
  auto image = smallSphere(unit);
  const QVector<double> regions{ 1.0 };
  auto raw = extractLabelMesh(image, regions, 0, 0.0);
  EXPECT_NEAR(enclosedVolume(raw), 257.0, 1e-6);
  for (int iterations : { 8, 16, kMaxSurfaceSmoothing }) {
    auto smooth = extractLabelMesh(image, regions, iterations, 0.0);
    const double volume = enclosedVolume(smooth);
    EXPECT_GT(volume, 0.95 * 257.0) << "iterations=" << iterations;
    EXPECT_LT(volume, 1.05 * 257.0) << "iterations=" << iterations;
    EXPECT_TRUE(smooth->GetPointData()->GetNormals());
  }
}

// Whatever the iteration count, no point leaves the voxel shell by more
// than half a voxel diagonal, measured in world units.
TEST(LabelMapSurfaceTest, SmoothingStaysWithinHalfAVoxel)
{
  const double spacing[3] = { 1.0, 1.0, 2.5 };
  auto image = smallSphere(spacing);
  const QVector<double> regions{ 1.0 };
  auto raw = extractLabelMesh(image, regions, 0, 0.0);
  auto smooth = extractLabelMesh(image, regions, 200, 0.0);
  ASSERT_EQ(smooth->GetNumberOfPoints(), raw->GetNumberOfPoints());
  const double limit = 0.5 * vtkMath::Norm(spacing);
  double moved = 0.0;
  for (vtkIdType i = 0; i < raw->GetNumberOfPoints(); ++i) {
    double a[3], b[3];
    raw->GetPoint(i, a);
    smooth->GetPoint(i, b);
    moved = std::max(moved, std::sqrt(vtkMath::Distance2BetweenPoints(a, b)));
  }
  EXPECT_GT(moved, 0.0) << "nothing was smoothed";
  EXPECT_LE(moved, limit + 1e-6);
}
