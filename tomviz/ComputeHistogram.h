/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizComputeHistogram_h
#define tomvizComputeHistogram_h

#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkPointData.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace tomviz {

// Mirrors the bin count in HistogramManager::PopulateHistogram.
constexpr int kHistogramBins = 256;

/**
 * Compute the finite value range used for histogram binning, reading the raw
 * buffer directly.
 *
 * This deliberately avoids vtkDataArray::GetRange()/GetFiniteRange(), which
 * lazily cache their result inside the array's vtkInformation object. That
 * cache write is NOT thread-safe: the histogram runs on a background thread
 * while the main (render) thread touches the same shared array, and concurrent
 * range caching corrupts the Information's reference counts -> "delete object
 * with non-zero reference count" -> crash in vtkGarbageCollector. Computing the
 * range from the buffer here keeps the background thread read-only with respect
 * to the shared array.
 *
 * \param useMagnitude when true and numComponents > 1, ranges over the vector
 *   magnitude (matching GetFiniteRange(range, -1) and the multi-component
 *   binning path in CalculateHistogram). Otherwise ranges over the raw
 *   component values (matching the per-component range used for the 2D
 *   histogram). For single-component arrays the two are equivalent.
 */
template <typename T>
void ComputeFiniteRange(const T* values, const vtkIdType numTuples,
                        const vtkIdType numComponents, const bool useMagnitude,
                        double range[2])
{
  double minValue = std::numeric_limits<double>::max();
  double maxValue = -std::numeric_limits<double>::max();

  if (useMagnitude && numComponents > 1) {
    for (vtkIdType j = 0; j < numTuples; ++j) {
      double squaredSum = 0.0;
      bool valid = true;
      for (vtkIdType c = 0; c < numComponents; ++c) {
        double value = static_cast<double>(values[j * numComponents + c]);
        if (!vtkMath::IsFinite(value)) {
          valid = false;
          break;
        }
        squaredSum += value * value;
      }
      if (valid) {
        double mag = std::sqrt(squaredSum);
        minValue = std::min(minValue, mag);
        maxValue = std::max(maxValue, mag);
      }
    }
  } else {
    const vtkIdType total = numTuples * numComponents;
    for (vtkIdType i = 0; i < total; ++i) {
      double value = static_cast<double>(values[i]);
      if (vtkMath::IsFinite(value)) {
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
      }
    }
  }

  if (minValue > maxValue) {
    // No finite values found; fall back to a benign range.
    minValue = 0.0;
    maxValue = 0.0;
  }

  range[0] = minValue;
  range[1] = maxValue;
}

/** Single component integral type specialization. */
template <typename T,
          typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
void calcHistogram(T* values, const vtkIdType numTuples, const float min,
                   const float inv, uint64_t* pops, int& invalid)
{
  // Clamp idx so garbage data (e.g. uninit numpy buffer pushed
  // before being filled) doesn't write past pops[].
  for (vtkIdType j = 0; j < numTuples; ++j) {
    int idx = static_cast<int>((*values++ - min) * inv);
    if (idx >= 0 && idx < kHistogramBins) {
      ++pops[idx];
    } else {
      ++invalid;
    }
  }
}

/** Needs to be present, should never be compiled. */
template <typename T>
void calcHistogram(T*, const vtkIdType, uint64_t*)
{
  static_assert(!std::is_same<unsigned char, T>::value, "Invalid type");
}

/** Single component unsigned char covering 0 -> 255 range. */
// inline: unlike its neighbours this overload is not a template, so
// without it the header cannot be included in more than one translation
// unit.
inline void calcHistogram(unsigned char* values, const vtkIdType numTuples,
                          uint64_t* pops)
{
  // unsigned char is always in [0, kBins-1], no clamp needed.
  for (vtkIdType j = 0; j < numTuples; ++j) {
    ++pops[*values++];
  }
}

/** Single component floating point type specialization. */
template <typename T,
          typename std::enable_if<!std::is_integral<T>::value>::type* = nullptr>
void calcHistogram(T* values, const vtkIdType numTuples, const float min,
                   const float inv, uint64_t* pops, int& invalid)
{
  for (vtkIdType j = 0; j < numTuples; ++j) {
    T value = *(values++);
    if (std::isfinite(value)) {
      int idx = static_cast<int>((value - min) * inv);
      if (idx >= 0 && idx < kHistogramBins) {
        ++pops[idx];
      } else {
        ++invalid;
      }
    } else {
      ++invalid;
    }
  }
}

/**
 * Computes a histogram from an array of values.
 * \param values The array from which to compute the histogram.
 * \param numTuples Number of tuples in the array.
 * \param numComponents Number of components in each tuple.
 * \param min Minimum value in range
 * \param max Maximum value in range
 * \param inv Inverse of bin size, numBins is the number of bins
 * in the histogram (or length of the pops array), and invalid is a return
 * parameter indicating how many values in the array had a non-finite value.
 */
template <typename T>
void CalculateHistogram(T* values, const vtkIdType numTuples,
                        const vtkIdType numComponents, const float min,
                        const float max, uint64_t* pops, const float inv,
                        int& invalid)
{
  // Single component is a simpler/faster path, let's dispatch separately.
  if (numComponents == 1) {
    // Very fast path for unsigned char in 0 -> 255 range, or fast path.
    if (std::is_same<T, unsigned char>::value && min == 0.f && max == 255.f) {
      calcHistogram(values, numTuples, pops);
    } else {
      calcHistogram(values, numTuples, min, inv, pops, invalid);
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
        int index = static_cast<int>((sqrt(squaredSum) - min) * inv);
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
  const size_t sizeBins = static_cast<size_t>(bins[0]) * bins[1];

  // Adjust histogram's spacing so that the axis show the actual range in the
  // chart
  double binSpacing[3] = { (range[1] - range[0]) / bins[0],
                           (range[1] * 0.25) / bins[1], 1.0 };
  histogram->SetSpacing(binSpacing);

  memset(histogramArr->GetVoidPointer(0), 0x0, sizeBins * sizeof(double));

  const size_t sizeSlice = static_cast<size_t>(dim[0]) * dim[1] * numComp;
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
      static_cast<size_t>(dim[0]) * dim[1] * kIndex * numComp;
    memcpy(&(sliceNext[0]), values + strideSlice, sizeSlice * sizeof(T));

    // Fill up temporary slices during the first two iterations
    if (kIndex >= 2) {
      for (int jIndex = 1; jIndex < dim[1] - 1; jIndex++) {
        for (int iIndex = 1; iIndex < dim[0] - 1; iIndex++) {
          const size_t centerIndex =
            static_cast<size_t>(dim[0]) * jIndex + iIndex;
          const size_t deltaXFront = centerIndex + 1;
          const size_t deltaXBack = centerIndex - 1;

          const double Dx = static_cast<double>(sliceCurrent[deltaXFront] -
                                                sliceCurrent[deltaXBack]) /
                            delta[0];

          const size_t deltaYFront =
            static_cast<size_t>(dim[0]) * (jIndex + 1) + iIndex;
          const size_t deltaYBack =
            static_cast<size_t>(dim[0]) * (jIndex - 1) + iIndex;
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

/**
 * Estimate the value below which @a fraction of the finite values (or
 * magnitudes, for multi-component arrays) lie, from a fine histogram
 * over @a range. Linear interpolation inside the bin holding the
 * percentile keeps the estimate smooth for coarse integer data.
 *
 * With @a excludeMinimum the values equal to range[0] are left out of
 * the count. A reconstruction is padded with its minimum (usually zero)
 * wherever there is nothing, and that pile can be most of the volume,
 * which drags every percentile down to the noise just above it.
 */
template <typename T>
double ComputePercentile(const T* values, const vtkIdType numTuples,
                         const vtkIdType numComponents, const double range[2],
                         const double fraction,
                         const bool excludeMinimum = false)
{
  constexpr int bins = 4096;
  if (numTuples <= 0 || !(range[1] > range[0])) {
    return range[0];
  }
  const double inv = bins / (range[1] - range[0]);
  std::vector<uint64_t> pops(bins, 0);
  uint64_t total = 0;
  for (vtkIdType j = 0; j < numTuples; ++j) {
    double value;
    if (numComponents == 1) {
      value = static_cast<double>(values[j]);
    } else {
      double squaredSum = 0.0;
      for (vtkIdType c = 0; c < numComponents; ++c) {
        double v = static_cast<double>(values[j * numComponents + c]);
        squaredSum += v * v;
      }
      value = std::sqrt(squaredSum);
    }
    if (!vtkMath::IsFinite(value) || (excludeMinimum && value == range[0])) {
      continue;
    }
    int idx = static_cast<int>((value - range[0]) * inv);
    idx = std::min(std::max(idx, 0), bins - 1);
    ++pops[idx];
    ++total;
  }
  if (total == 0) {
    return range[0];
  }

  const double target = std::min(std::max(fraction, 0.0), 1.0) * total;
  uint64_t below = 0;
  for (int i = 0; i < bins; ++i) {
    if (below + pops[i] >= target) {
      double within = pops[i] > 0 ? (target - below) / pops[i] : 0.0;
      return range[0] + (i + within) / inv;
    }
    below += pops[i];
  }
  return range[1];
}

} // namespace tomviz

#endif
