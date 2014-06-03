/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "vtkStreamingWorker.h"

#include "vtkAppendPolyData.h"
#include "vtkDataArray.h"
#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkNew.h"

#include "SubdividedVolume.h"

#include "tbb/tbb_thread.h"
#include "tbb/spin_mutex.h"

#include <iostream>

namespace
{
  typedef tbb::spin_mutex MutexType;

#define temTemplateMacro(call)                                              \
  vtkTemplateMacroCase(VTK_FLOAT, float, call);                             \
  vtkTemplateMacroCase(VTK_INT, int, call);                                 \
  vtkTemplateMacroCase(VTK_UNSIGNED_INT, unsigned int, call);               \
  vtkTemplateMacroCase(VTK_SHORT, short, call);                             \
  vtkTemplateMacroCase(VTK_UNSIGNED_SHORT, unsigned short, call);           \
  vtkTemplateMacroCase(VTK_CHAR, char, call);                               \
  vtkTemplateMacroCase(VTK_UNSIGNED_CHAR, unsigned char, call)

#define temDataArrayIteratorMacro(_array, _call)                           \
  temTemplateMacro(                                                        \
    vtkAbstractArray *_aa(_array);                                         \
    if (vtkDataArrayTemplate<VTK_TT> *_dat =                               \
        vtkDataArrayTemplate<VTK_TT>::FastDownCast(_aa))                   \
      {                                                                    \
      typedef VTK_TT vtkDAValueType;                                       \
      typedef vtkDataArrayTemplate<vtkDAValueType> vtkDAContainerType;     \
      typedef vtkDAContainerType::Iterator vtkDAIteratorType;              \
      vtkDAIteratorType vtkDABegin(_dat->Begin());                         \
      vtkDAIteratorType vtkDAEnd(_dat->End());                             \
      _call;                                                               \
      }                                                                    \
    )

template<typename LoggerType>
struct ComputeFunctor
{
  vtkStreamingWorker::AlgorithmMode Mode;
  TEM::accel::SubdividedVolume& Volume;
  vtkSmartPointer<vtkAppendPolyData>& Appender;
  MutexType* AppenderMutex;
  LoggerType& Logger;
  bool& ContinueWorking;
  bool& FinishedWorkingOnData;

  //----------------------------------------------------------------------------
  ComputeFunctor(vtkStreamingWorker::AlgorithmMode mode,
                 TEM::accel::SubdividedVolume& volume,
                 vtkSmartPointer<vtkAppendPolyData>& appender,
                 MutexType* appenderMutex,
                 bool& keepProcessing,
                 bool& finishedWorkingOnData,
                 LoggerType& logger):
    Mode(mode),
    Volume(volume),
    Appender(appender),
    AppenderMutex(appenderMutex),
    Logger(logger),
    ContinueWorking(keepProcessing),
    FinishedWorkingOnData(finishedWorkingOnData)
  {
  }

  //----------------------------------------------------------------------------
  template<typename ValueType>
  void operator()(double v, ValueType)
  {

  //wrap up this->Volume.method as a function argument
  if(this->Mode == vtkStreamingWorker::CONTOUR)
    {
    TEM::accel::ContourFunctor functor(this->Volume);
    this->run(functor, v, ValueType());
    }
  else if(this->Mode == vtkStreamingWorker::THRESHOLD)
    {
    TEM::accel::ThresholdFunctor functor(this->Volume);
    this->run(functor, v, ValueType());
    }
  }

  //----------------------------------------------------------------------------
  template<typename Functor, typename ValueType>
  void run(Functor functor, double v, ValueType)
  {
  dax::cont::Timer<> timer;
  const std::size_t totalSubGrids = this->Volume.numSubGrids();
  vtkSmartPointer< vtkPolyData> output;

  bool haveMoreData=false;
  for(std::size_t i=0; i < totalSubGrids && this->ContinueWorking; ++i)
    {
    if(this->ContinueWorking && this->Volume.isValidSubGrid(i, v))
      {
      output = functor(v, i, ValueType(), this->Logger );

        //lock while we add to the appender
        {
        haveMoreData=true;
        MutexType::scoped_lock lock(*this->AppenderMutex);
        this->Appender->AddInputDataObject( output );
        }
      }
    if(i%50==0 && haveMoreData && this->ContinueWorking)
      {
      MutexType::scoped_lock lock(*this->AppenderMutex);
      this->Appender->Update();
      haveMoreData = false;
      }
    }

  //append any remaining subgrids
  if(haveMoreData && this->ContinueWorking)
    {
    MutexType::scoped_lock lock(*this->AppenderMutex);
    this->Appender->Update();
    haveMoreData = false;
    }

  this->Logger << "algorithm time: " << timer.GetElapsedTime() << std::endl;
  this->FinishedWorkingOnData = true;
  }
};


}

vtkStandardNewMacro(vtkStreamingWorker)

//----------------------------------------------------------------------------
class vtkStreamingWorker::WorkerInternals
{
public:
  //----------------------------------------------------------------------------
  WorkerInternals(std::size_t numSubGridsPerDim):
    Thread(),
    ContinueWorking(false),
    FinishedWorkingOnData(false),
    CurrentRenderDataFinished(false),
    Volume(),
    ComputedData(),
    CurrentRenderData(),
    NumSubGridsPerDim(numSubGridsPerDim)
  {
    this->ComputedData = vtkSmartPointer<vtkAppendPolyData>::New();
    this->CurrentRenderData = vtkSmartPointer<vtkPolyData>::New();
  }

  //----------------------------------------------------------------------------
  ~WorkerInternals()
  {
    //only try to tell the thread to stop if we actually have a thread running
    if(this->Thread.joinable())
      {
      //tell the thread to stop
      this->ContinueWorking = false;

      //wait for the thread to stop
      this->Thread.join();
      }
  }

  //----------------------------------------------------------------------------
  bool IsValid() const { return this->Volume.numSubGrids() > 0; }

  //----------------------------------------------------------------------------
  void StopWork() { this->ContinueWorking = false; }

  //----------------------------------------------------------------------------
  bool IsFinished() const
  {
    //we need a better check to have this be able to handle the use
    //case that we have finished contouring before ProcessViewRequest
    //with REQUEST_STREAMING_UPDATE has been called and we report we are
    //finished before we ever report we have started.
    //I think the best way is to track that GetFinishedPieces() has been
    //called AFTER the algorithm is finished, only than are we really 'finished'

    return this->CurrentRenderDataFinished;
  }

  //----------------------------------------------------------------------------
  template<class IteratorType, class LoggerType>
  bool Run(vtkStreamingWorker::AlgorithmMode mode,
           vtkImageData* input,
           double isoValue,
           IteratorType begin,
           IteratorType end,
           LoggerType& logger)
  {
    typedef typename std::iterator_traits<IteratorType>::value_type ValueType;

    //first check if we have an existing thread
    if(this->Thread.joinable())
      {
      this->ContinueWorking = false;
      this->Thread.join();
      }

    //construct a thread, swap it with the class thread
    //and make it run the code
    this->ContinueWorking = true;
    this->FinishedWorkingOnData = false;
    this->CurrentRenderDataFinished = false;

    //clear the appender
    this->ComputedData = vtkSmartPointer<vtkAppendPolyData>::New();

    if(this->Volume.numSubGrids() == 0)
      {
      logger << "CreateSearchStructure" << std::endl;
      this->Volume = TEM::accel::SubdividedVolume( this->NumSubGridsPerDim,
                                                   input,
                                                   logger );

      logger << "ComputeHighLows" << std::endl;
      this->Volume.ComputeHighLows( begin, end, logger );
      }


    //now give the thread the volume to contour
    ComputeFunctor<LoggerType> functor(mode,
                                       this->Volume,
                                       this->ComputedData,
                                       &this->ComputedDataMutex,
                                       this->ContinueWorking,
                                       this->FinishedWorkingOnData,
                                       logger);
    tbb::tbb_thread t(functor, isoValue, ValueType());
    this->Thread.swap(t);

    return true;
  }

//----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> GetFinishedPieces()
{
  MutexType::scoped_lock lock(this->ComputedDataMutex);
  if(this->ComputedData->GetNumberOfInputPorts() > 0)
    {
    CurrentRenderData->ShallowCopy(ComputedData->GetOutputDataObject(0));
    }
  this->CurrentRenderDataFinished = this->FinishedWorkingOnData;

  return this->CurrentRenderData;
}

private:
  tbb::tbb_thread Thread;
  bool ContinueWorking;
  bool FinishedWorkingOnData;
  bool CurrentRenderDataFinished;

  TEM::accel::SubdividedVolume Volume;
  vtkSmartPointer<vtkAppendPolyData> ComputedData;
  vtkSmartPointer<vtkPolyData> CurrentRenderData;
  MutexType ComputedDataMutex;

  std::size_t NumSubGridsPerDim;
};

//----------------------------------------------------------------------------
vtkStreamingWorker::vtkStreamingWorker():
  Internals( new vtkStreamingWorker::WorkerInternals(6) )
{
}

//----------------------------------------------------------------------------
vtkStreamingWorker::~vtkStreamingWorker()
{
  delete this->Internals;
}

//----------------------------------------------------------------------------
void vtkStreamingWorker::StartContour(vtkImageData* image,
                                      vtkDataArray* data,
                                      double isoValue)
{
  //bad input abort
  if(!image||!data)
    {
    return;
    }
  //todo we need to spawn a thread here I believe
  switch (data->GetDataType())
      {
      temDataArrayIteratorMacro( data,
            this->Internals->Run(CONTOUR, image, isoValue, vtkDABegin, vtkDAEnd, std::cout) );
      default:
        break;
      }
}

//----------------------------------------------------------------------------
void vtkStreamingWorker::StartThreshold(vtkImageData* image,
                                      vtkDataArray* data,
                                      double isoValue)
{
  //bad input abort
  if(!image||!data)
    {
    return;
    }
  //todo we need to spawn a thread here I believe
  switch (data->GetDataType())
      {
      temDataArrayIteratorMacro( data,
            this->Internals->Run(THRESHOLD, image, isoValue, vtkDABegin, vtkDAEnd, std::cout) );
      default:
        break;
      }
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData>
vtkStreamingWorker::GetFinishedPieces()
{
  return this->Internals->GetFinishedPieces();
}

//----------------------------------------------------------------------------
void vtkStreamingWorker::StopWork()
{
  this->Internals->StopWork();
}

//----------------------------------------------------------------------------
bool vtkStreamingWorker::IsFinished() const
{
  return this->Internals->IsFinished();
}

//----------------------------------------------------------------------------
bool vtkStreamingWorker::AlreadyComputedSearchStructure() const
{
  return this->Internals->IsValid();
}

//------------------------------------------------------------------------------
void vtkStreamingWorker::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
