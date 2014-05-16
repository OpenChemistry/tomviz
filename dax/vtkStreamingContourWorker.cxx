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
#include "vtkStreamingContourWorker.h"

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
struct ComputeContourFunctor
{
  TEM::accel::SubdividedVolume& Volume;
  vtkSmartPointer<vtkAppendPolyData>& Appender;
  MutexType* AppenderMutex;
  LoggerType& Logger;
  bool& ContinueContouring;
  bool& FinishedContouring;

  //----------------------------------------------------------------------------
  ComputeContourFunctor(TEM::accel::SubdividedVolume& volume,
                        vtkSmartPointer<vtkAppendPolyData>& appender,
                        MutexType* appenderMutex,
                        bool& keepProcessing,
                        bool& finishedContouring,
                        LoggerType& logger):
    Volume(volume),
    Appender(appender),
    AppenderMutex(appenderMutex),
    Logger(logger),
    ContinueContouring(keepProcessing),
    FinishedContouring(finishedContouring)
  {
  }

  //----------------------------------------------------------------------------
  template<typename ValueType>
  void operator()(double v, ValueType)
  {
  this->Logger << "Contour with value: " << v << std::endl;
  std::size_t numTriangles=0;

  dax::cont::Timer<> timer;
  const std::size_t totalSubGrids = this->Volume.numSubGrids();

  for(std::size_t i=0; i < totalSubGrids && this->ContinueContouring; ++i)
    {
    if(this->ContinueContouring && this->Volume.isValidSubGrid(i, v))
      {

      vtkSmartPointer< vtkPolyData> tris =
                this->Volume.ContourSubGrid(v, i, ValueType(), this->Logger );
      numTriangles += tris->GetNumberOfPolys();

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

  this->Logger << "contour: " << timer.GetElapsedTime() << " num tris " << numTriangles << std::endl;
  this->FinishedContouring = true;
  }
};


}

vtkStandardNewMacro(vtkStreamingContourWorker)

//----------------------------------------------------------------------------
class vtkStreamingContourWorker::WorkerInternals
{
public:
  //----------------------------------------------------------------------------
  WorkerInternals(std::size_t numSubGridsPerDim):
    Thread(),
    ContinueContouring(false),
    FinishedContouring(false),
    Volume(),
    ComputedContours(),
    CurrentRenderData(),
    NumSubGridsPerDim(numSubGridsPerDim)
  {
    this->ComputedContours = vtkSmartPointer<vtkAppendPolyData>::New();
  }

  //----------------------------------------------------------------------------
  ~WorkerInternals()
  {
    //tell the thread to stop
    this->ContinueContouring = false;

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
      return this->FinishedContouring;
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
      this->ContinueContouring = false;
      this->Thread.join();
      }

    //construct a thread, swap it with the class thread
    //and make it run the code
    this->ContinueContouring = true;
    this->FinishedContouring = false;

    //clear the appender
    this->ComputedContours = vtkSmartPointer<vtkAppendPolyData>::New();

    logger << "CreateSearchStructure" << std::endl;
    this->Volume = TEM::accel::SubdividedVolume( this->NumSubGridsPerDim,
                                                 input,
                                                 logger );

    logger << "ComputeHighLows" << std::endl;
    this->Volume.ComputeHighLows( begin, end, logger );


    //now give the thread the volume to contour
    ComputeContourFunctor<LoggerType> functor(this->Volume,
                                              this->ComputedContours,
                                              &this->ComputedContoursMutex,
                                              this->ContinueContouring,
                                              this->FinishedContouring,
                                              logger);
    tbb::tbb_thread t(functor, isoValue, ValueType());
    this->Thread.swap(t);

    return true;

  }
//----------------------------------------------------------------------------
vtkDataObject* GetFinishedPieces()
{
  MutexType::scoped_lock lock(this->ComputedContoursMutex);
  if(this->ComputedContours->GetNumberOfInputPorts() > 0)
    {
    std::cout << "calling GetFinishedPieces yet again" << std::endl;
    CurrentRenderData->ShallowCopy(ComputedContours->GetOutputDataObject(0));
    }
  return this->CurrentRenderData.GetPointer();
}

private:
  tbb::tbb_thread Thread;
  bool ContinueContouring;
  bool FinishedContouring;

  TEM::accel::SubdividedVolume Volume;
  vtkSmartPointer<vtkAppendPolyData> ComputedContours;
  vtkNew<vtkPolyData> CurrentRenderData;
  MutexType ComputedContoursMutex;

  std::size_t NumSubGridsPerDim;
};

//----------------------------------------------------------------------------
vtkStreamingContourWorker::vtkStreamingContourWorker():
  Internals( new vtkStreamingContourWorker::WorkerInternals(8) )
{
}

//----------------------------------------------------------------------------
vtkStreamingContourWorker::~vtkStreamingContourWorker()
{
  delete this->Internals;
}

//----------------------------------------------------------------------------
void vtkStreamingContourWorker::Start(vtkImageData* image,
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
vtkStreamingContourWorker::GetFinishedPieces()
{
  return this->Internals->GetFinishedPieces();
}

//----------------------------------------------------------------------------
bool vtkStreamingContourWorker::IsFinished() const
{
  return this->Internals->IsFinished();
}

//------------------------------------------------------------------------------
void vtkStreamingContourWorker::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
