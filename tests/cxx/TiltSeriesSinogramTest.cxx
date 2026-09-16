/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkUnsignedShortArray.h>

#include "TomographyTiltSeries.h"

#include <vector>

using namespace tomviz;

namespace {

// A wider-than-tall tilt series (x > y), the shape that #2308 crashed on.
constexpr int kX = 300;
constexpr int kY = 200;
constexpr int kZ = 4;
constexpr int kNray = 64;

vtkSmartPointer<vtkImageData> makeWideTiltSeries()
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(kX, kY, kZ);
  image->AllocateScalars(VTK_UNSIGNED_SHORT, 1);
  auto* ptr = static_cast<unsigned short*>(image->GetScalarPointer());
  for (int z = 0; z < kZ; ++z) {
    for (int y = 0; y < kY; ++y) {
      for (int x = 0; x < kX; ++x) {
        ptr[(z * kY + y) * kX + x] =
          static_cast<unsigned short>((x + 7 * y + 1000 * z) % 65535);
      }
    }
  }
  return image;
}

} // namespace

// With a vertical tilt axis the slice index runs along y, so a slice seeded
// from the x dimension (0.75 * 300 = 225 > 200) used to read past the end
// of the tilt series. It must now clamp to the last valid y slice.
TEST(TiltSeriesSinogram, verticalTiltAxisClampsSliceToYDim)
{
  auto image = makeWideTiltSeries();
  std::vector<float> outOfRange(kNray * kZ, -1.0f);
  std::vector<float> lastValid(kNray * kZ, -2.0f);

  TomographyTiltSeries::getSinogram(image, 225, outOfRange.data(), kNray,
                                    0.0, /*tiltAxis=*/1);
  TomographyTiltSeries::getSinogram(image, kY - 1, lastValid.data(), kNray,
                                    0.0, /*tiltAxis=*/1);

  EXPECT_EQ(outOfRange, lastValid);
}

TEST(TiltSeriesSinogram, horizontalTiltAxisClampsSliceToXDim)
{
  auto image = makeWideTiltSeries();
  std::vector<float> outOfRange(kNray * kZ, -1.0f);
  std::vector<float> lastValid(kNray * kZ, -2.0f);

  TomographyTiltSeries::getSinogram(image, kX + 50, outOfRange.data(), kNray,
                                    0.0, /*tiltAxis=*/0);
  TomographyTiltSeries::getSinogram(image, kX - 1, lastValid.data(), kNray,
                                    0.0, /*tiltAxis=*/0);

  EXPECT_EQ(outOfRange, lastValid);
}

TEST(TiltSeriesSinogram, negativeSliceClampsToZero)
{
  auto image = makeWideTiltSeries();
  std::vector<float> negative(kNray * kZ, -1.0f);
  std::vector<float> zero(kNray * kZ, -2.0f);

  TomographyTiltSeries::getSinogram(image, -5, negative.data(), kNray, 0.0,
                                    /*tiltAxis=*/1);
  TomographyTiltSeries::getSinogram(image, 0, zero.data(), kNray, 0.0,
                                    /*tiltAxis=*/1);

  EXPECT_EQ(negative, zero);
}

// In-range slices still gather the expected samples: with no axis shift the
// ray at the centre of the sinogram lands on the middle of the tilt axis.
TEST(TiltSeriesSinogram, inRangeSliceMatchesSource)
{
  auto image = makeWideTiltSeries();
  std::vector<float> sino(kNray * kZ, 0.0f);
  const int slice = 150; // along y for a vertical tilt axis
  TomographyTiltSeries::getSinogram(image, slice, sino.data(), kNray, 0.0,
                                    /*tiltAxis=*/1);

  // Ray Nray/2 has rayCoord 0 -> index1 = xDim/2 with weight1 = 0, so the
  // sample is exactly the pixel at x = xDim/2 + 1 (index2, weight2 = 1).
  auto* ptr = static_cast<unsigned short*>(image->GetScalarPointer());
  for (int z = 0; z < kZ; ++z) {
    const float expected = ptr[(z * kY + slice) * kX + kX / 2 + 1];
    EXPECT_FLOAT_EQ(sino[z * kNray + kNray / 2], expected) << "tilt " << z;
  }
}
