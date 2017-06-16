#ifndef tomvizComputeHistogram_h
#define tomvizComputeHistogram_h

#ifdef DAX_DEVICE_ADAPTER
#include "dax/Worklets.h"
#include <dax/cont/ArrayHandle.h>
#include <dax/cont/ArrayHandleCounting.h>
#include <dax/cont/DispatcherMapField.h>
#else
#include "vtkMath.h"
#endif
#include "vtkDoubleArray.h"
#include "vtkImageData.h"

namespace tomviz {
#ifdef DAX_DEVICE_ADAPTER

namespace worklets {
template <typename T>
struct ScalarRange : dax::exec::WorkletMapField
{
  typedef void ControlSignature(FieldOut);
  typedef _1 ExecutionSignature(WorkId);

  DAX_CONT_EXPORT
  ScalarRange(T* v, int len, int numWorkers)
    : Values(v), Length(len), TaskSize(len / numWorkers), NumWorkers(numWorkers)
  {
  }

  DAX_EXEC_EXPORT
  dax::Tuple<double, 2> operator()(dax::Id id) const
  {
    // compute the range we are calculating
    const T* myValue = this->Values + (id * TaskSize);

    dax::Id end_offset;
    if (id + 1 == this->NumWorkers) {
      end_offset = this->Length;
    } else {
      end_offset = (id + 1) * TaskSize;
    }
    const T* myEnd = this->Values + end_offset;

    dax::Tuple<T, 2> lh;
    lh[0] = myValue[0];
    lh[1] = myValue[0];

    for (myValue++; myValue != myEnd; myValue++) {
      lh[0] = std::min(lh[0], *myValue);
      lh[1] = std::max(lh[1], *myValue);
    }

    return dax::Tuple<double, 2>(lh[0], lh[1]);
  }
  T* Values;
  int Length;
  int TaskSize;
  int NumWorkers;
};

template <typename T>
struct Histogram : dax::exec::WorkletMapField
{
  typedef void ControlSignature(FieldIn);
  typedef void ExecutionSignature(_1);

  DAX_CONT_EXPORT
  Histogram(T* v, int valueLength, int numWorkers, int* histo, float minValue,
            int numBins, float binSize)
    : Length(valueLength), TaskSize(valueLength / numWorkers),
      NumWorkers(numWorkers), NumBins(numBins), MinValue(minValue),
      BinSize(binSize), Values(v), GlobalHisto(histo)
  {
  }

  DAX_EXEC_EXPORT
  void operator()(dax::Id id) const
  {
    std::vector<int> histo(this->NumBins, 0);

    const int maxBin(this->NumBins - 1);

    const T* myValue = this->Values + (id * TaskSize);

    dax::Id end_offset;
    if (id + 1 == this->NumWorkers) {
      end_offset = this->Length;
    } else {
      end_offset = (id + 1) * TaskSize;
    }

    const T* myEnd = this->Values + end_offset;

    for (; myValue != myEnd; myValue++) {
      const int index =
        std::min(maxBin, static_cast<int>((*myValue - MinValue) / BinSize));
      ++histo[index];
    }

    // using tbb atomics add my histo to the global histogram
    tbb::atomic<int> x;
    for (int i = 0; i < this->NumBins; ++i) {
      x = GlobalHisto[i];
      x.fetch_and_add(histo[i]);
      GlobalHisto[i] = x;
    }
  }

  int Length;
  int TaskSize;
  int NumWorkers;
  int NumBins;
  float MinValue;
  float BinSize;
  T* Values;
  int* GlobalHisto;
};
}

template <typename T>
void GetScalarRange(T* values, const unsigned int n, double* minmax)
{
  using namespace dax::cont;
  const int numTasks = 32;

  ArrayHandle<dax::Tuple<double, 2>> minmaxHandle;
  minmaxHandle.PrepareForOutput(numTasks);

  worklets::ScalarRange<T> worklet(values, n, numTasks);
  DispatcherMapField<worklets::ScalarRange<T>> dispatcher(worklet);
  dispatcher.Invoke(minmaxHandle);

  // reduce the minmaxHandle
  ArrayHandle<dax::Tuple<double, 2>>::PortalConstControl portal =
    minmaxHandle.GetPortalConstControl();
  minmax[0] = portal.Get(0)[0];
  minmax[1] = portal.Get(0)[1];
  for (unsigned int j = 1; j < minmaxHandle.GetNumberOfValues(); ++j) {
    minmax[0] = std::min(portal.Get(j)[0], minmax[0]);
    minmax[1] = std::max(portal.Get(j)[1], minmax[1]);
  }
}

template <typename T>
void CalculateHistogram(T* values, const unsigned int n, const float min,
                        int* pops, const float inc, const int numberOfBins)
{
  using namespace dax::cont;
  const int numTasks = 32;

  worklets::Histogram<T> worklet(values, n, numTasks, pops, min, numberOfBins,
                                 inc);
  DispatcherMapField<worklets::Histogram<T>> dispatcher(worklet);
  dispatcher.Invoke(make_ArrayHandleCounting<dax::Id>(0, numTasks));
}

#else
template <typename T>
void GetScalarRange(T* values, const vtkIdType n, double* minmax)
{
  T tempMinMax[2];
  tempMinMax[0] = values[0];
  tempMinMax[1] = values[0];
  for (vtkIdType j = 1; j < n; ++j) {
    // This code does not handle NaN or Inf values, so check for them
    if (!vtkMath::IsFinite(values[j]))
      continue;
    tempMinMax[0] = std::min(values[j], tempMinMax[0]);
    tempMinMax[1] = std::max(values[j], tempMinMax[1]);
  }

  minmax[0] = static_cast<double>(tempMinMax[0]);
  minmax[1] = static_cast<double>(tempMinMax[1]);
}

template <typename T>
void CalculateHistogram(T* values, const vtkIdType n, const float min,
                        int* pops, const float inc, const int numberOfBins,
                        int& invalid)
{
  const int maxBin(numberOfBins - 1);
  for (vtkIdType j = 0; j < n; ++j) {
    // This code does not handle NaN or Inf values, so check for them and handle
    // them specially
    if (vtkMath::IsFinite(*values)) {
      int index = std::min(static_cast<int>((*(values++) - min) / inc), maxBin);
      ++pops[index];
    } else {
      ++values;
      ++invalid;
    }
  }
}

template <typename T>
void Calculate2DHistogram(T* values, const int* dim, const int numComp,
                          const double* range, vtkImageData* histogram, double spacing[3])
{
  // Assumes all inputs are valid
  // Expects histogram image to be 1C double
  vtkDataArray* arr = histogram->GetPointData()->GetScalars();
  vtkDoubleArray* histogramArr = vtkDoubleArray::SafeDownCast(arr);

  int bins[3];
  histogram->GetDimensions(bins);
  const size_t sizeBins = static_cast<size_t>(bins[0] * bins[1]);

  // Adjust histogram's spacing so that the axis show the actual range in the chart
  double binSpacing[3] = { (range[1] - range[0])/ bins[0],
    (range[1] * 0.25)/ bins[1], 
    1.0 };
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
  const double delta[3] = {spacing[0] * 2 / avgSpacing,
    spacing[1] * 2 / avgSpacing,
    spacing[2] * 2 / avgSpacing};

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
                                                sliceCurrent[deltaXBack]) / delta[0];

          const size_t deltaYFront = dim[0] * (jIndex + 1) + iIndex;
          const size_t deltaYBack = dim[0] * (jIndex - 1) + iIndex;
          const double Dy = static_cast<double>(sliceCurrent[deltaYFront] -
                                                sliceCurrent[deltaYBack]) / delta[1];

          const double Dz = static_cast<double>(sliceNext[centerIndex] -
                                                sliceLast[centerIndex]) / delta[2];

          double gradMag = sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
          gradMagMax = vtkMath::Max(gradMag, gradMagMax);
          gradMagMin = vtkMath::Min(gradMag, gradMagMin);

          // Normalize to RangeMax/4. This is what the gradient computation in the
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
          double histogramValue = histogramArr->GetTuple1(tupleIndex);
          histogramArr->SetTuple1(tupleIndex, ++histogramValue);
        }
      }
    }

    std::swap(sliceLast, sliceCurrent);
    std::swap(sliceCurrent, sliceNext);
  }
}

#endif
}

#endif
