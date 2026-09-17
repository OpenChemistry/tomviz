/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "sinks/ExplodedGeometry.h"

#include <cmath>

using tomviz::pipeline::explodedDirection;
using tomviz::pipeline::explodedExtent;
using tomviz::pipeline::kExplodedCustomAxis;

TEST(ExplodedGeometryTest, AxesAreUnitVectorsAndCustomIsNormalized)
{
  const std::array<double, 3> custom = { 3.0, 0.0, 4.0 };
  EXPECT_EQ(explodedDirection(0, custom), (std::array<double, 3>{ 1, 0, 0 }));
  EXPECT_EQ(explodedDirection(2, custom), (std::array<double, 3>{ 0, 0, 1 }));
  auto dir = explodedDirection(kExplodedCustomAxis, custom);
  EXPECT_DOUBLE_EQ(dir[0], 0.6);
  EXPECT_DOUBLE_EQ(dir[1], 0.0);
  EXPECT_DOUBLE_EQ(dir[2], 0.8);
  // A zero vector cannot be normalized; +Z is the harmless choice
  EXPECT_EQ(explodedDirection(kExplodedCustomAxis, { 0, 0, 0 }),
            (std::array<double, 3>{ 0, 0, 1 }));
}

TEST(ExplodedGeometryTest, OffsetIsInVoxelsAndKeepsEverySlabAVoxelThick)
{
  using tomviz::pipeline::explodedOffsetLimit;
  using tomviz::pipeline::explodedShift;
  using tomviz::pipeline::explodedVoxelStep;

  const double spacing[3] = { 1.0, 2.0, 4.0 };
  EXPECT_DOUBLE_EQ(explodedVoxelStep({ 1, 0, 0 }, spacing), 1.0);
  EXPECT_DOUBLE_EQ(explodedVoxelStep({ 0, 0, 1 }, spacing), 4.0);
  const double s = 1.0 / std::sqrt(2.0);
  EXPECT_NEAR(explodedVoxelStep({ s, s, 0 }, spacing),
              std::sqrt(0.5 + 2.0), 1e-12);

  // 100 long, 4 slabs of 25, voxels of 2: 24 voxels of room per slab
  // minus the one that must stay, so 11
  EXPECT_EQ(explodedOffsetLimit(100.0, 4, 2.0), 11);
  EXPECT_EQ(explodedOffsetLimit(100.0, 4, 30.0), 0);
  EXPECT_EQ(explodedOffsetLimit(100.0, 4, 0.0), 0);
  EXPECT_DOUBLE_EQ(explodedShift(3, 100.0, 4, 2.0), 6.0);
  EXPECT_DOUBLE_EQ(explodedShift(-3, 100.0, 4, 2.0), -6.0);
  EXPECT_DOUBLE_EQ(explodedShift(50, 100.0, 4, 2.0), 22.0);
  EXPECT_DOUBLE_EQ(explodedShift(-50, 100.0, 4, 2.0), -22.0);
}

TEST(ExplodedGeometryTest, ExtentMatchesTheBoxAlongAnAxisAndItsDiagonal)
{
  const double bounds[6] = { 0.0, 10.0, -5.0, 5.0, 2.0, 6.0 };
  double lo = 0.0;
  double length = 0.0;
  explodedExtent(bounds, { 1, 0, 0 }, lo, length);
  EXPECT_DOUBLE_EQ(lo, -5.0);
  EXPECT_DOUBLE_EQ(length, 10.0);
  explodedExtent(bounds, { 0, 0, 1 }, lo, length);
  EXPECT_DOUBLE_EQ(lo, -2.0);
  EXPECT_DOUBLE_EQ(length, 4.0);

  // Along the diagonal of a cube the span is the diagonal's length
  const double cube[6] = { 0.0, 2.0, 0.0, 2.0, 0.0, 2.0 };
  const double s = 1.0 / std::sqrt(3.0);
  explodedExtent(cube, { s, s, s }, lo, length);
  EXPECT_NEAR(length, 2.0 * std::sqrt(3.0), 1e-12);
  EXPECT_NEAR(lo, -std::sqrt(3.0), 1e-12);
  // Sign of the direction does not change the span
  explodedExtent(cube, { -s, -s, -s }, lo, length);
  EXPECT_NEAR(length, 2.0 * std::sqrt(3.0), 1e-12);
}
