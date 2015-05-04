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

template<typename GridType>
void combine_grids(std::vector<GridType>& grids,
                   std::vector<dax::Vector3>& points,
                   std::vector<dax::Id>& cellConns)
{
  typedef typename std::vector<GridType>::iterator IteratorType;
  const std::size_t existingNumPoints = points.size();
  const std::size_t existingNumCellIds = cellConns.size();

  std::size_t numNewPoints = 0;
  std::size_t numNewCellIds = 0;
  for(IteratorType grid = grids.begin(); grid != grids.end(); ++grid)
    {
    numNewPoints += static_cast<std::size_t>(grid->GetNumberOfPoints());
    //we need the size of the cell connection array not the num of cells
    numNewCellIds += static_cast<std::size_t>(
                              grid->GetCellConnections().GetNumberOfValues());
    }

  //resize the vectors
  points.resize( points.size() + numNewPoints );
  cellConns.resize( cellConns.size() + numNewCellIds );

  std::size_t pointOffset = existingNumPoints;
  std::size_t cellIdOffset = existingNumCellIds;
  for(IteratorType grid = grids.begin(); grid != grids.end(); ++grid)
    {
    //copy the points, and than move the offset pointer to account for the
    //new points we have copied in
    grid->GetPointCoordinates().CopyInto(points.begin() + pointOffset);


    //copy the cell conns, and than move the offset pointer to account for the
    //new cell conns we have copied in
    grid->GetCellConnections().CopyInto(cellConns.begin() + cellIdOffset);

    //fixup the newly copied ids, by adding the number of points we
    //already have to each new id
    const std::size_t numValuesToFixUp = static_cast<std::size_t>(
                              grid->GetCellConnections().GetNumberOfValues());
    for(std::size_t i = 0; i < numValuesToFixUp; ++i)
      {
      //these ids are point ids, so we need to increment by the pointOffset
      //not the celloffset
      cellConns[cellIdOffset+i] += pointOffset;
    }

    pointOffset += static_cast<std::size_t>(grid->GetNumberOfPoints());
    cellIdOffset += numValuesToFixUp;
    }

  //now that the values in grids are in points and cells, we clear grids.
  grids.clear();
}


template<typename LoggerType>
struct ComputeFunctor
{
  vtkStreamingWorker::AlgorithmMode Mode;
  TEM::accel::SubdividedVolume& Volume;
  vtkSmartPointer<vtkPolyData>& VTKOutputData;
  MutexType* OutputDataMutex;
  LoggerType& Logger;
  bool& ContinueWorking;
  bool& FinishedWorkingOnData;

  //----------------------------------------------------------------------------
  ComputeFunctor(vtkStreamingWorker::AlgorithmMode mode,
                 TEM::accel::SubdividedVolume& volume,
                 vtkSmartPointer<vtkPolyData>& outputData,
                 MutexType* outputDaxMutex,
                 bool& keepProcessing,
                 bool& finishedWorkingOnData,
                 LoggerType& logger):
    Mode(mode),
    Volume(volume),
    VTKOutputData(outputData),
    OutputDataMutex(outputDaxMutex),
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
  else if(this->Mode == vtkStreamingWorker::POINTCLOUD)
    {
    TEM::accel::PointCloudFunctor functor(this->Volume);
    this->run(functor, v, ValueType());
    }
  }

  //----------------------------------------------------------------------------
  template<typename Functor, typename ValueType>
  void run(Functor functor, double v, ValueType)
  {
  dax::cont::Timer<> timer;
  dax::cont::Timer<> conv_timer; double dataconv_time = 0;

  const std::size_t totalSubGrids = this->Volume.numSubGrids();
  typedef typename Functor::ReturnType OutputGridType;

  //need a special temp grid type since OutputGridType can use virtual topology
  //and we can't use that when we are copying concrete cellConns
  typedef dax::cont::UnstructuredGrid< typename OutputGridType::CellTag > TempGridType;

  //lightweight storage of combined points and cells
  std::vector<OutputGridType> gridsPendingCollection;
  std::vector<dax::Vector3> points;
  std::vector<dax::Id> cellConns;

  bool haveMoreData=false;
  for(std::size_t i=0; i < totalSubGrids && this->ContinueWorking; ++i)
    {
    if(this->ContinueWorking && this->Volume.isValidSubGrid(i, v))
      {
      OutputGridType outputGrid = functor(v, i, ValueType(), this->Logger );
      gridsPendingCollection.push_back(outputGrid);

      haveMoreData=true;
      }
    if(i%50==0 && haveMoreData && this->ContinueWorking)
      {
      conv_timer.Reset();
      combine_grids(gridsPendingCollection,points,cellConns);

      //make a dax grid structure around the std vector(s), this is basically
      //a free operation
      TempGridType tempGrid( dax::cont::make_ArrayHandle(cellConns),
                             dax::cont::make_ArrayHandle(points));

      MutexType::scoped_lock lock(*this->OutputDataMutex);
      convertPoints(tempGrid,this->VTKOutputData);
      convertCells(tempGrid,this->VTKOutputData);
      dataconv_time += conv_timer.GetElapsedTime();

      haveMoreData = false;
      }
    }

  //append any remaining subgrids
  if(haveMoreData && this->ContinueWorking)
    {
    conv_timer.Reset();
    combine_grids(gridsPendingCollection,points,cellConns);

    //make a dax grid structure around the std vector(s), this is basically
    //a free operation
    TempGridType tempGrid( dax::cont::make_ArrayHandle(cellConns),
                           dax::cont::make_ArrayHandle(points));

    MutexType::scoped_lock lock(*this->OutputDataMutex);
    convertPoints(tempGrid,this->VTKOutputData);
    convertCells(tempGrid,this->VTKOutputData);
    dataconv_time += conv_timer.GetElapsedTime();

    haveMoreData = false;
    }

  double full_time = timer.GetElapsedTime();
  double alg_time = full_time - dataconv_time;
  this->Logger << "algorithm time: " << alg_time << std::endl;
  this->Logger << "data conversion time: " << dataconv_time << std::endl;
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
    this->ComputedData = vtkSmartPointer<vtkPolyData>::New();
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
    this->ComputedData = vtkSmartPointer<vtkPolyData>::New();

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
  if(this->ComputedData->GetNumberOfCells() > 0 &&
     this->ComputedData->GetNumberOfPoints() > 0)
    {
    CurrentRenderData->ShallowCopy(this->ComputedData);
    }
  this->CurrentRenderDataFinished = this->FinishedWorkingOnData;
  std::cout << "GetFinishedPieces" << std::endl;

  return this->CurrentRenderData;
}

private:
  tbb::tbb_thread Thread;
  bool ContinueWorking;
  bool FinishedWorkingOnData;
  bool CurrentRenderDataFinished;

  TEM::accel::SubdividedVolume Volume;
  vtkSmartPointer<vtkPolyData> ComputedData;
  vtkSmartPointer<vtkPolyData> CurrentRenderData;
  MutexType ComputedDataMutex;

  std::size_t NumSubGridsPerDim;
};

//----------------------------------------------------------------------------
vtkStreamingWorker::vtkStreamingWorker():
  Internals( new vtkStreamingWorker::WorkerInternals(8) ),
  ValidWorkerInput(true)
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
    this->ValidWorkerInput = false;
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
    this->ValidWorkerInput = false;
    return;
    }
  //todo we need to spawn a thread here I believe
  switch (data->GetDataType())
      {
      temDataArrayIteratorMacro( data,
            this->Internals->Run(POINTCLOUD, image, isoValue, vtkDABegin, vtkDAEnd, std::cout) );
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
  //if we didn't get valid input and output arguments we are finished
  //since we can't do anything
  if(!ValidWorkerInput)
    {
    return true;
    }
  else
    {
    return this->Internals->IsFinished();
    }
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
