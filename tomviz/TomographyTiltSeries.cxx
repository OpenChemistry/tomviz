/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TomographyTiltSeries.h"
#include "vtkDataArray.h"
#include "vtkFieldData.h"
#include "vtkImageData.h"
#include <math.h>

#include <algorithm>
#define PI 3.14159265359
#include "vtkFloatArray.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"

#include <QDebug>

namespace {

// conversion code
template <typename T>
vtkSmartPointer<vtkFloatArray> convertToFloatT(T* data, vtkIdType len)
{
  vtkSmartPointer<vtkFloatArray> array = vtkSmartPointer<vtkFloatArray>::New();
  array->SetNumberOfTuples(len);
  float* f = static_cast<float*>(array->GetVoidPointer(0));
  for (vtkIdType i = 0; i < len; ++i) {
    f[i] = (float)data[i];
  }
  return array;
}

vtkSmartPointer<vtkFloatArray> convertToFloat(vtkImageData* image)
{
  vtkDataArray* scalars = image->GetPointData()->GetScalars();
  vtkIdType len = scalars->GetNumberOfTuples();
  vtkSmartPointer<vtkFloatArray> array;
  switch (scalars->GetDataType()) {
    vtkTemplateMacro(array = convertToFloatT(
                       static_cast<VTK_TT*>(scalars->GetVoidPointer(0)), len););
  }
  return array;
}
} // end of namespace

namespace tomviz {

namespace TomographyTiltSeries {

void getSinogram(vtkImageData* tiltSeries, int sliceNumber, float* sinogram)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  const vtkIdType xDim = extents[1] - extents[0] + 1; // Number of slices
  const vtkIdType yDim = extents[3] - extents[2] + 1; // Number of rays
  const vtkIdType zDim = extents[5] - extents[4] + 1; // Number of tilts

  // Keep the requested slice inside the volume rather than reading past it.
  sliceNumber =
    static_cast<int>(std::clamp<vtkIdType>(sliceNumber, 0, xDim - 1));

  // Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float* dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(
    0)); // Get pointer to tilt series (of type float)

  // Extract sinograms from tilt series. Make a deep copy
  for (vtkIdType t = 0; t < zDim; ++t) // Loop through tilts (z-direction)
  {
    for (vtkIdType r = 0; r < yDim; ++r) // Loop through rays (y-direction)
    {
      sinogram[t * yDim + r] =
        dataPtr[t * xDim * yDim + r * xDim + sliceNumber];
    }
  }
}

// Extract sinograms from tilt series
void getSinogram(vtkImageData* tiltSeries, int sliceNumber, float* sinogram,
                 int Nray, double axisPosition, int tiltAxis)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  const vtkIdType xDim = extents[1] - extents[0] + 1; // number of slices
  const vtkIdType yDim = extents[3] - extents[2] + 1; // number of rays
  const vtkIdType zDim = extents[5] - extents[4] + 1; // number of tilts

  // Note that the meaning of x and y flip if the tiltAxis is flipped
  vtkIdType tiltAxDim;
  if (tiltAxis == 0)
    tiltAxDim = yDim;
  else
    tiltAxDim = xDim;

  // sliceNumber runs along the axis the tilt axis is not, so it must stay
  // below that axis' length. A slice past the end reads beyond the float
  // copy of the tilt series (#2308).
  const vtkIdType sliceAxDim = (tiltAxis == 0) ? xDim : yDim;
  sliceNumber =
    static_cast<int>(std::clamp<vtkIdType>(sliceNumber, 0, sliceAxDim - 1));

  // Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float* dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(
    0)); // Get pointer to tilt series (of type float)

  double rayWidth = (double)tiltAxDim / (double)Nray;
  std::vector<float> weight1(Nray); // Store weights for linear interpolation
  std::vector<float> weight2(Nray); // Store weights for linear interpolation
  std::vector<int> index1(Nray);    // Store indices for linear interpolation
  std::vector<int> index2(Nray);    // Store indices for linear interpolation

  // Extract sinograms from tilt series. Make a deep copy
  for (vtkIdType z = 0; z < zDim; ++z) // Loop through tilts (z-direction)
  {
    for (int r = 0; r < Nray; ++r) // Loop through rays (y-direction)
    {
      if (z == 0) { // Initialize weights and indices
        double rayCoord = (double)(r - Nray / 2) * rayWidth + axisPosition;
        index1[r] = floor(rayCoord) + tiltAxDim / 2;
        index2[r] = index1[r] + 1;
        weight1[r] = fabs(rayCoord - floor(rayCoord));
        weight2[r] = 1 - weight1[r];
      }
      sinogram[z * Nray + r] = 0;
      vtkIdType dataInd;
      if (index1[r] >= 0 && index1[r] < tiltAxDim) {
        if (tiltAxis == 0)
          dataInd = z * xDim * yDim + index1[r] * xDim + sliceNumber;
        else
          dataInd = z * xDim * yDim + sliceNumber * xDim + index1[r];

        sinogram[z * Nray + r] += dataPtr[dataInd] * weight1[r];
      }
      if (index2[r] >= 0 && index2[r] < tiltAxDim) {
        if (tiltAxis == 0)
          dataInd = z * xDim * yDim + index2[r] * xDim + sliceNumber;
        else
          dataInd = z * xDim * yDim + sliceNumber * xDim + index2[r];

        sinogram[z * Nray + r] += dataPtr[dataInd] * weight2[r];
      }
    }
  }
}

void averageTiltSeries(vtkImageData* tiltSeries, float* average)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  const vtkIdType xDim = extents[1] - extents[0] + 1; // Number of slices
  const vtkIdType yDim = extents[3] - extents[2] + 1; // Number of rays
  const vtkIdType zDim = extents[5] - extents[4] + 1; // Number of tilts

  // Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float* dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(
    0)); // Get pointer to tilt series (of type float)

  for (vtkIdType z = 0; z < zDim; ++z) {
    for (vtkIdType y = 0; y < yDim; ++y) {
      for (vtkIdType x = 0; x < xDim; ++x) {
        if (z == 0)
          average[y * xDim + x] = 0;

        average[y * xDim + x] += dataPtr[z * xDim * yDim + y * xDim + x];

        if (z == zDim - 1) // Normalize
          average[y * xDim + x] /= zDim;
      }
    }
  }
}

} // end of namespace TomographyTiltSeries
} // end of namespace tomviz
