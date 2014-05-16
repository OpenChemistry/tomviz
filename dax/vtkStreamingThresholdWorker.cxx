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
#include "vtkStreamingThresholdWorker.h"

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
struct ComputeThresholdFunctor
{
  TEM::accel::SubdividedVolume& Volume;
  vtkSmartPointer<vtkAppendPolyData>& Appender;
  MutexType* AppenderMutex;
  LoggerType& Logger;
  bool& ContinueThresholding;
  bool& FinishedThresholding;

  //----------------------------------------------------------------------------
  ComputeThresholdFunctor(TEM::accel::SubdividedVolume& volume,
                        vtkSmartPointer<vtkAppendPolyData>& appender,
                        MutexType* appenderMutex,
                        bool& keepProcessing,
                        bool& finishedThresholding,
                        LoggerType& logger):
    Volume(volume),
    Appender(appender),
    AppenderMutex(appenderMutex),
    Logger(logger),
    ContinueThresholding(keepProcessing),
    FinishedThresholding(finishedThresholding)
  {
  }

  //----------------------------------------------------------------------------
  template<typename ValueType>
  void operator()(double v, ValueType)
  {
  this->Logger << "Threshold with value: " << v << std::endl;
  std::size_t numVerts=0;

  dax::cont::Timer<> timer;
  const std::size_t totalSubGrids = this->Volume.numSubGrids();

  for(std::size_t i=0; i < totalSubGrids && this->ContinueThresholding; ++i)
    {
    if(this->ContinueThresholding  && this->Volume.isValidSubGrid(i, v))
      {

      vtkSmartPointer< vtkPolyData> tris =
                this->Volume.PointCloudSubGrid(v, i, ValueType(), this->Logger );
      numVerts += tris->GetNumberOfVerts();

        //lock while we add to the appender
        {
        MutexType::scoped_lock lock(*this->AppenderMutex);
        this->Appender->AddInputDataObject( tris );
        }
      }
    if(i%50==0 && this->Appender->GetNumberOfInputPorts() > 0)
      {
      MutexType::scoped_lock lock(*this->AppenderMutex);
      this->Appender->Update();
      }
    }

  //append any remaining subgrids
  if(this->Appender->GetNumberOfInputPorts() > 0)
    {
    MutexType::scoped_lock lock(*this->AppenderMutex);
    this->Appender->Update();
    }

  this->Logger << "Threshold: " << timer.GetElapsedTime() << " num verts " << numVerts << std::endl;
  this->FinishedThresholding = true;
  }
};


}

vtkStandardNewMacro(vtkStreamingThresholdWorker)

//----------------------------------------------------------------------------
class vtkStreamingThresholdWorker::WorkerInternals
{
public:
  //----------------------------------------------------------------------------
  WorkerInternals(std::size_t numSubGridsPerDim):
    Thread(),
    ContinueThresholding(false),
    FinishedThresholding(false),
    Volume(),
    ComputedThresholds(),
    CurrentRenderData(),
    NumSubGridsPerDim(numSubGridsPerDim)
  {
    this->ComputedThresholds = vtkSmartPointer<vtkAppendPolyData>::New();
  }

  //----------------------------------------------------------------------------
  ~WorkerInternals()
  {
    //tell the thread to stop
    this->ContinueThresholding = false;

    //wait for the thread to stop
    this->Thread.join();
  }

  //----------------------------------------------------------------------------
  bool IsValid() const { return this->Volume.numSubGrids() > 0; }

  //----------------------------------------------------------------------------
  bool IsFinished() const
  {
    if(this->IsValid())
      {
      return this->FinishedThresholding;
      }
    return false;
  }

  //----------------------------------------------------------------------------
  template<class IteratorType, class LoggerType>
  bool Run(vtkImageData* input,
           double isoValue,
           IteratorType begin,
           IteratorType end,
           LoggerType& logger)
  {

    typedef typename std::iterator_traits<IteratorType>::value_type ValueType;

    //first check if we have an existing thread
    if(this->Thread.joinable())
      {
      this->ContinueThresholding = false;
      this->Thread.join();
      }

    //construct a thread, swap it with the class thread
    //and make it run the code
    this->ContinueThresholding = true;
    this->FinishedThresholding = false;

    //clear the appender
    this->ComputedThresholds = vtkSmartPointer<vtkAppendPolyData>::New();

    logger << "CreateSearchStructure" << std::endl;
    this->Volume = TEM::accel::SubdividedVolume( this->NumSubGridsPerDim,
                                                 input,
                                                 logger );

    logger << "ComputeHighLows" << std::endl;
    this->Volume.ComputeHighLows( begin, end, logger );


    //now give the thread the volume to Threshold
    ComputeThresholdFunctor<LoggerType> functor(this->Volume,
                                              this->ComputedThresholds,
                                              &this->ComputedThresholdsMutex,
                                              this->ContinueThresholding,
                                              this->FinishedThresholding,
                                              logger);
    tbb::tbb_thread t(functor, isoValue, ValueType());
    this->Thread.swap(t);

    return true;

  }
//----------------------------------------------------------------------------
vtkDataObject* GetFinishedPieces()
{
  MutexType::scoped_lock lock(this->ComputedThresholdsMutex);
  if(this->ComputedThresholds->GetNumberOfInputPorts() > 0)
    {
    std::cout << "calling GetFinishedPieces yet again" << std::endl;
    CurrentRenderData->ShallowCopy(ComputedThresholds->GetOutputDataObject(0));
    }
  return this->CurrentRenderData.GetPointer();
}

private:
  tbb::tbb_thread Thread;
  bool ContinueThresholding;
  bool FinishedThresholding;

  TEM::accel::SubdividedVolume Volume;
  vtkSmartPointer<vtkAppendPolyData> ComputedThresholds;
  vtkNew<vtkPolyData> CurrentRenderData;
  MutexType ComputedThresholdsMutex;

  std::size_t NumSubGridsPerDim;
};

//----------------------------------------------------------------------------
vtkStreamingThresholdWorker::vtkStreamingThresholdWorker():
  Internals( new vtkStreamingThresholdWorker::WorkerInternals(8) )
{
}

//----------------------------------------------------------------------------
vtkStreamingThresholdWorker::~vtkStreamingThresholdWorker()
{
  delete this->Internals;
}

//----------------------------------------------------------------------------
void vtkStreamingThresholdWorker::Start(vtkImageData* image,
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
            this->Internals->Run(image, isoValue, vtkDABegin, vtkDAEnd, std::cout) );
      default:
        break;
      }
}

//----------------------------------------------------------------------------
vtkDataObject*
vtkStreamingThresholdWorker::GetFinishedPieces()
{
  return this->Internals->GetFinishedPieces();
}

//----------------------------------------------------------------------------
bool vtkStreamingThresholdWorker::IsFinished() const
{
  return this->Internals->IsFinished();
}

//------------------------------------------------------------------------------
void vtkStreamingThresholdWorker::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
