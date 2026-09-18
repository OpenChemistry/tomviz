/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "pipeline/data/LabelMapData.h"
#include "pipeline/sinks/LabelMapSurface.h"

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>

#include <QColor>

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
