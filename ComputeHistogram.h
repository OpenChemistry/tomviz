#ifndef __ComputeHistogram_h
#define __ComputeHistogram_h

#ifdef DAX_DEVICE_ADAPTER
#include <dax/cont/ArrayHandle.h>
#include <dax/cont/ArrayHandleCounting.h>
#include <dax/cont/DispatcherMapField.h>
#endif

namespace TEM
{
#ifdef DAX_DEVICE_ADAPTER

namespace worklets
{
template<typename T>
struct ScalarRange: dax::exec::WorkletMapField
{
  typedef void ControlSignature(FieldOut);
  typedef _1 ExecutionSignature(WorkId);

  DAX_CONT_EXPORT
  ScalarRange(T* v, int len, int numWorkers):
    Values(v),
    Length(len),
    TaskSize(len/numWorkers),
    NumWorkers(numWorkers)
  {}

  DAX_EXEC_EXPORT
  dax::Tuple<double,2> operator()(dax::Id id) const
  {
    //compute the range we are calculating
    const T* myValue = this->Values + (id * TaskSize);

    dax::Id end_offset;
    if(id+1 == this->NumWorkers)
      { end_offset = this->Length; }
    else
      { end_offset = (id+1)*TaskSize; }
    const T* myEnd = this->Values + end_offset;

    dax::Tuple<T,2> lh;
    lh[0] = myValue[0];
    lh[1] = myValue[0];

    for(myValue++; myValue != myEnd; myValue++)
      {
      lh[0] = std::min(lh[0],*myValue);
      lh[1] = std::max(lh[1],*myValue);
      }

    return dax::Tuple<double,2>(lh[0],lh[1]);
  }
  T* Values;
  int Length;
  int TaskSize;
  int NumWorkers;
};

template<typename T>
struct Histogram: dax::exec::WorkletMapField
{
  typedef void ControlSignature(FieldIn);
  typedef void ExecutionSignature(_1);

  DAX_CONT_EXPORT
  Histogram(T* v, int valueLength, int numWorkers,
            int* histo, float minValue, int numBins, float binSize):
    Length(valueLength),
    TaskSize(valueLength/numWorkers),
    NumWorkers(numWorkers),
    NumBins(numBins),
    MinValue(minValue),
    BinSize(binSize),
    Values(v),
    GlobalHisto(histo)
    {}

  DAX_EXEC_EXPORT
  void operator()(dax::Id id) const
  {
    std::vector<int> histo(this->NumBins, 0);

    const int maxBin(this->NumBins - 1);

    const T* myValue = this->Values + (id * TaskSize);

    dax::Id end_offset;
    if(id+1 == this->NumWorkers)
      { end_offset = this->Length; }
    else
      { end_offset = (id+1)*TaskSize; }

    const T* myEnd = this->Values + end_offset;

    for(; myValue != myEnd; myValue++)
      {
      const int index = std::min(maxBin,
                     static_cast<int>((*myValue - MinValue) / BinSize));
      ++histo[index];
      }

    //using tbb atomics add my histo to the global histogram
    tbb::atomic<int> x;
    for(int i=0; i < this->NumBins; ++i)
      {
      x = GlobalHisto[i];
      x.fetch_and_add( histo[ i ]);
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

template<typename T>
void GetScalarRange(T *values, const unsigned int n, double* minmax)
{
  using namespace dax::cont;
  const int numTasks = 32;

  ArrayHandle< dax::Tuple<double,2> > minmaxHandle;
  minmaxHandle.PrepareForOutput(numTasks);

  worklets::ScalarRange<T> worklet(values, n, numTasks);
  DispatcherMapField< worklets::ScalarRange<T> > dispatcher(worklet);
  dispatcher.Invoke( minmaxHandle );

  //reduce the minmaxHandle
  ArrayHandle< dax::Tuple<double,2> >::PortalConstControl portal =
                                        minmaxHandle.GetPortalConstControl();
  minmax[0] = portal.Get(0)[0];
  minmax[1] = portal.Get(0)[1];
  for (unsigned int j = 1; j < minmaxHandle.GetNumberOfValues(); ++j)
    {
    minmax[0] = std::min(portal.Get(j)[0], minmax[0]);
    minmax[1] = std::max(portal.Get(j)[1], minmax[1]);
    }
}

template<typename T>
void CalculateHistogram(T *values, const unsigned int n, const float min,
                        int *pops, const float inc, const int numberOfBins)
{
  using namespace dax::cont;
  const int numTasks = 32;

  worklets::Histogram<T> worklet(values, n, numTasks, pops,  min, numberOfBins, inc);
  DispatcherMapField< worklets::Histogram<T> > dispatcher(worklet);
  dispatcher.Invoke( make_ArrayHandleCounting<dax::Id>(0,numTasks) );
}

#else
template<typename T>
void GetScalarRange(T *values, const unsigned int n, double* minmax)
{
  T tempMinMax[2];
  tempMinMax[0] = values[0];
  tempMinMax[1] = values[0];
  for (unsigned int j = 1; j < n; ++j)
    {
    tempMinMax[0] = std::min(values[j], tempMinMax[0]);
    tempMinMax[1] = std::max(values[j], tempMinMax[1]);
    }

  minmax[0] = static_cast<double>(tempMinMax[0]);
  minmax[1] = static_cast<double>(tempMinMax[1]);
}

template<typename T>
void CalculateHistogram(T *values, const unsigned int n, const float min,
                        int *pops, const float inc, const int numberOfBins)
{
  const int maxBin(numberOfBins - 1);
  for (unsigned int j = 0; j < n; ++j)
    {
    int index = std::min(static_cast<int>((*(values++) - min) / inc), maxBin);
    ++pops[index];
    }
}
#endif

}

#endif