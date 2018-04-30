#ifndef tomvizComputeHistogram_h
#define tomvizComputeHistogram_h

#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkMath.h>

namespace tomviz {

/**
 * Computes a histogram from an array of values.
 * \param values The array from which to compute the histogram.
 * \param numTuples Number of tuples in the array.
 * \param numComponents Number of components in each tuple.
 * \param component The desired component from each tuple to use when building
 *   the histogram (-1 means compute the histogram of the L2
 *   norm of each tuple)
 * \param inc Bin size, numBins is the number of bins
 * in the histogram (or length of the pops array), and invalid is a return
 * parameter indicating how many values in the array had a non-finite value.
 */
template <typename T>
void CalculateHistogram(T* values, const vtkIdType numTuples,
                        const vtkIdType numComponents, int component,
                        const float min, int* pops, const float inc,
                        const int numBins, int& invalid)
{
  const int maxBin(numBins - 1);

  // Simplify the case where tuple magnitude is requested but the number of
  // components is only 1.
  if (component == -1 && numComponents == 1) {
    component = 0;
  }

  if (component >= 0) {
    // Single scalar value
    for (vtkIdType j = 0; j < numTuples; ++j) {
      // This code does not handle NaN or Inf values, so check for them and
      // handle
      // them specially
      T value = *(values + component);
      if (vtkMath::IsFinite(value)) {
        int index = std::min(static_cast<int>((value - min) / inc), maxBin);
        ++pops[index];
      } else {
        ++invalid;
      }
      values += numComponents;
    }
  } else {
    // Multicomponent magnitude
    for (vtkIdType j = 0; j < numTuples; ++j) {
      // Check that all components are valid.
      bool valid = true;
      double squaredSum = 0.0;
      for (vtkIdType c = 0; c < numComponents; ++c) {
        T value = *(values + c);
        if (!vtkMath::IsFinite(value)) {
          valid = false;
          break;
        }
        squaredSum += (value * value);
      }
      if (valid) {
        int index =
          std::min(static_cast<int>((sqrt(squaredSum) - min) / inc), maxBin);
        ++pops[index];
      } else {
        ++invalid;
      }
      values += numComponents;
    }
  }
}

template <typename T>
void Calculate2DHistogram(T* values, const int* dim, const int numComp,
                          const double* range, vtkImageData* histogram,
                          double spacing[3])
{
  // Assumes all inputs are valid
  // Expects histogram image to be 1C double
  vtkDataArray* arr = histogram->GetPointData()->GetScalars();
  using ArrDouble = vtkAOSDataArrayTemplate<double>;
  ArrDouble* histogramArr = ArrDouble::SafeDownCast(arr);

  int bins[3];
  histogram->GetDimensions(bins);
  const size_t sizeBins = static_cast<size_t>(bins[0] * bins[1]);

  // Adjust histogram's spacing so that the axis show the actual range in the
  // chart
  double binSpacing[3] = { (range[1] - range[0]) / bins[0],
                           (range[1] * 0.25) / bins[1], 1.0 };
  histogram->SetSpacing(binSpacing);

  memset(histogramArr->GetVoidPointer(0), 0x0, sizeBins * sizeof(double));

  const size_t sizeSlice = static_cast<size_t>(dim[0] * dim[1] * numComp);
  std::vector<T> sliceLast(sizeSlice, 0);
  std::vector<T> sliceCurrent(sizeSlice, 0);
  std::vector<T> sliceNext(sizeSlice, 0);

  double gradMagMax = std::numeric_limits<double>::min();
  double gradMagMin = std::numeric_limits<double>::max();

  // Central differences delta (2 * h)
  const double avgSpacing = (spacing[0] + spacing[1] + spacing[2]) / 3.0;
  const double delta[3] = { spacing[0] * 2 / avgSpacing,
                            spacing[1] * 2 / avgSpacing,
                            spacing[2] * 2 / avgSpacing };

  for (int kIndex = 0; kIndex < dim[2]; kIndex++) {
    // Index assumes alignment order in  x -> y -> z.
    // ( z0 * Dx * Dy + y0 * Dx + x0 ) * numComp
    const size_t strideSlice =
      static_cast<size_t>(dim[0] * dim[1] * kIndex * numComp);
    memcpy(&(sliceNext[0]), values + strideSlice, sizeSlice * sizeof(T));

    // Fill up temporary slices during the first two iterations
    if (kIndex >= 2) {
      for (int jIndex = 1; jIndex < dim[1] - 1; jIndex++) {
        for (int iIndex = 1; iIndex < dim[0] - 1; iIndex++) {
          const size_t centerIndex = dim[0] * jIndex + iIndex;
          const size_t deltaXFront = centerIndex + 1;
          const size_t deltaXBack = centerIndex - 1;

          const double Dx = static_cast<double>(sliceCurrent[deltaXFront] -
                                                sliceCurrent[deltaXBack]) /
                            delta[0];

          const size_t deltaYFront = dim[0] * (jIndex + 1) + iIndex;
          const size_t deltaYBack = dim[0] * (jIndex - 1) + iIndex;
          const double Dy = static_cast<double>(sliceCurrent[deltaYFront] -
                                                sliceCurrent[deltaYBack]) /
                            delta[1];

          const double Dz = static_cast<double>(sliceNext[centerIndex] -
                                                sliceLast[centerIndex]) /
                            delta[2];

          double gradMag = sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
          gradMagMax = vtkMath::Max(gradMag, gradMagMax);
          gradMagMin = vtkMath::Min(gradMag, gradMagMin);

          // Normalize to RangeMax/4. This is what the gradient computation in
          // the
          // GPUMapper's fragment shader expects.
          const double maxGradMag = range[1] * 0.25;
          gradMag = floor(gradMag + 0.5);
          gradMag = vtkMath::ClampValue(gradMag, 0.0, maxGradMag);
          const vtkIdType gradIndex =
            static_cast<vtkIdType>(gradMag * (bins[1] - 1) / maxGradMag);

          const T value = values[strideSlice + centerIndex * numComp];
          const vtkIdType valueIndex = static_cast<vtkIdType>(
            (value - range[0]) * (bins[1] - 1) / (range[1] - range[0]));

          // Update histogram array
          const vtkIdType tupleIndex = gradIndex * bins[0] + valueIndex;
          double histogramValue = histogramArr->GetValue(tupleIndex);
          histogramArr->SetValue(tupleIndex, ++histogramValue);
        }
      }
    }

    std::swap(sliceLast, sliceCurrent);
    std::swap(sliceCurrent, sliceNext);
  }
}

} // namespace tomviz

#endif
