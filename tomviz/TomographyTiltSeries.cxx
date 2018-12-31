/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TomographyTiltSeries.h"
#include "vtkDataArray.h"
#include "vtkFieldData.h"
#include "vtkImageData.h"
#include <math.h>
#define PI 3.14159265359
#include "vtkFloatArray.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"

#include <QDebug>

namespace {

// conversion code
template <typename T>
vtkSmartPointer<vtkFloatArray> convertToFloatT(T* data, int len)
{
  vtkSmartPointer<vtkFloatArray> array = vtkSmartPointer<vtkFloatArray>::New();
  array->SetNumberOfTuples(len);
  float* f = static_cast<float*>(array->GetVoidPointer(0));
  for (int i = 0; i < len; ++i) {
    f[i] = (float)data[i];
  }
  return array;
}

vtkSmartPointer<vtkFloatArray> convertToFloat(vtkImageData* image)
{
  vtkDataArray* scalars = image->GetPointData()->GetScalars();
  int len = scalars->GetNumberOfTuples();
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
  int xDim = extents[1] - extents[0] + 1; // Number of slices
  int yDim = extents[3] - extents[2] + 1; // Number of rays
  int zDim = extents[5] - extents[4] + 1; // Number of tilts

  // Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float* dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(
    0)); // Get pointer to tilt series (of type float)

  // Extract sinograms from tilt series. Make a deep copy
  for (int t = 0; t < zDim; ++t) // Loop through tilts (z-direction)
  {
    for (int r = 0; r < yDim; ++r) // Loop through rays (y-direction)
    {
      sinogram[t * yDim + r] =
        dataPtr[t * xDim * yDim + r * xDim + sliceNumber];
    }
  }
}

// Extract sinograms from tilt series
void getSinogram(vtkImageData* tiltSeries, int sliceNumber, float* sinogram,
                 int Nray, double axisPosition)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; // number of slices
  int yDim = extents[3] - extents[2] + 1; // number of rays in tilt series
  int zDim = extents[5] - extents[4] + 1; // number of tilts

  // Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float* dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(
    0)); // Get pointer to tilt series (of type float)

  double rayWidth = (double)yDim / (double)Nray;
  std::vector<float> weight1(Nray); // Store weights for linear interpolation
  std::vector<float> weight2(Nray); // Store weights for linear interpolation
  std::vector<int> index1(Nray);    // Store indices for linear interpolation
  std::vector<int> index2(Nray);    // Store indices for linear interpolation

  // Extract sinograms from tilt series. Make a deep copy
  for (int z = 0; z < zDim; ++z) // Loop through tilts (z-direction)
  {
    for (int r = 0; r < Nray; ++r) // Loop through rays (y-direction)
    {
      if (z == 0) { // Initialize weights and indicies
        double rayCoord = (double)(r - Nray / 2) * rayWidth + axisPosition;
        index1[r] = floor(rayCoord) + yDim / 2;
        index2[r] = index1[r] + 1;
        weight1[r] = fabs(rayCoord - floor(rayCoord));
        weight2[r] = 1 - weight1[r];
      }
      sinogram[z * Nray + r] = 0;
      if (index1[r] >= 0 && index1[r] < yDim)
        sinogram[z * Nray + r] +=
          dataPtr[z * xDim * yDim + index1[r] * xDim + sliceNumber] *
          weight1[r];
      if (index2[r] >= 0 && index2[r] < yDim)
        sinogram[z * Nray + r] +=
          dataPtr[z * xDim * yDim + index2[r] * xDim + sliceNumber] *
          weight2[r];
    }
  }
}

void averageTiltSeries(vtkImageData* tiltSeries, float* average)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; // Number of slices
  int yDim = extents[3] - extents[2] + 1; // Number of rays in tilt series
  int zDim = extents[5] - extents[4] + 1; // Number of tilts

  // Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float* dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(
    0)); // Get pointer to tilt series (of type float)

  for (int z = 0; z < zDim; ++z) {
    for (int y = 0; y < yDim; ++y) {
      for (int x = 0; x < xDim; ++x) {
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
