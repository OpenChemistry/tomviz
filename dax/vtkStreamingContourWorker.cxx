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

#include "vtkDataSet.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"

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
  std::vector< vtkSmartPointer<vtkPolyData> >& DataSets;
  MutexType* DataSetsMutex;
  LoggerType& Logger;
  bool& ContinueContouring;

  //----------------------------------------------------------------------------
  ComputeContourFunctor(TEM::accel::SubdividedVolume& volume,
                        std::vector< vtkSmartPointer<vtkPolyData> >& ds,
                        MutexType* dsMutex,
                        bool& keepProcessing,
                        LoggerType& logger):
    Volume(volume),
    DataSets(ds),
    DataSetsMutex(dsMutex),
    Logger(logger),
    ContinueContouring(keepProcessing)
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

  vtkSmartPointer< vtkPolyData > tris;
  for(std::size_t i=0; i < totalSubGrids && this->ContinueContouring; ++i)
    {
    if(this->ContinueContouring && this->Volume.isValidSubGrid(i, v))
      {
      tris = this->Volume.ContourSubGrid(v, i, ValueType(), this->Logger );
      numTriangles += tris->GetNumberOfPolys();

        //lock while we push_back
        {
        MutexType::scoped_lock lock(*this->DataSetsMutex);
        this->DataSets.push_back( tris );
        }

      }
    }

  this->Logger << "contour: " << timer.GetElapsedTime()  << std::endl;
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
    Volume(),
    ComputedContours(),
    NumSubGridsPerDim(numSubGridsPerDim)
  {
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
                                              logger);
    tbb::tbb_thread t(functor, isoValue, ValueType());
    this->Thread.swap(t);

    return true;

  }
//----------------------------------------------------------------------------
std::vector< vtkSmartPointer< vtkPolyData > > GetFinishedPieces()
{
  const std::size_t numElementsFinished = this->ComputedContours.size();

  //copy the elements already in the vector to a new vector
  std::vector< vtkSmartPointer<vtkPolyData> > finishedData(
                          this->ComputedContours.begin(),
                          this->ComputedContours.begin()+numElementsFinished);


  { //lock while we erase
    MutexType::scoped_lock lock(this->ComputedContoursMutex);
    this->ComputedContours.erase(this->ComputedContours.begin(),
                          this->ComputedContours.begin()+numElementsFinished);
  }

  return finishedData;
}

private:
  tbb::tbb_thread Thread;
  bool ContinueContouring;

  TEM::accel::SubdividedVolume Volume;
  std::vector< vtkSmartPointer<vtkPolyData> > ComputedContours;
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
std::vector< vtkSmartPointer< vtkPolyData > >
vtkStreamingContourWorker::GetFinishedPieces()
{
  return this->Internals->GetFinishedPieces();
}

//----------------------------------------------------------------------------
bool vtkStreamingContourWorker::IsFinished() const
{
  return false;
}

//------------------------------------------------------------------------------
void vtkStreamingContourWorker::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
