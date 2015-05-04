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

#include <dax/cont/DeviceAdapter.h>
#include <dax/cont/ArrayHandle.h>
#include <dax/cont/ArrayHandleCounting.h>
#include <dax/cont/ArrayHandleImplicit.h>
#include <dax/cont/DispatcherMapCell.h>
#include <dax/cont/DispatcherMapField.h>
#include <dax/cont/DispatcherGenerateInterpolatedCells.h>
#include <dax/cont/Timer.h>

#include "vtkImageData.h"
#include "vtkNew.h"

#include "Worklets.h"

//for std::swap (c++98 / c++11 have different locations for swap)
#include <algorithm>
#include <utility>


namespace TEM
{
namespace accel
{
//----------------------------------------------------------------------------
SubdividedVolume::SubdividedVolume( ):
  Origin(),
  Spacing(),
  Extent(),
  SubGrids(),
  SubGridCellIJKOffset(),
  PerSubGridLowHighs(),
  PerSubGridValues()
{

}

//----------------------------------------------------------------------------
template< typename ImageDataType, typename LoggerType >
SubdividedVolume::SubdividedVolume( std::size_t desiredSubGridsPerDim,
                                    ImageDataType* data,
                                    LoggerType& logger )
{
  dax::cont::Timer<> timer;

  double vtk_origin[3];  data->GetOrigin(vtk_origin);
  double vtk_spacing[3]; data->GetSpacing(vtk_spacing);
  int    vtk_extent[6];  data->GetExtent(vtk_extent);

  this->Origin  = dax::Vector3(vtk_origin[0],vtk_origin[1],vtk_origin[2]);
  this->Spacing = dax::Vector3(vtk_spacing[0],vtk_spacing[1],vtk_spacing[2]);
  this->Extent  = dax::Extent3(dax::make_Id3(vtk_extent[0],vtk_extent[2],vtk_extent[4]),
                               dax::make_Id3(vtk_extent[1],vtk_extent[3],vtk_extent[5]));

  const dax::Id3 cellDims = dax::extentCellDimensions(this->Extent);

  dax::Id3 subGridsPerDim;
  for(std::size_t i=0; i < 3; ++i)
    {
    subGridsPerDim[i] =( (desiredSubGridsPerDim < cellDims[i]) ? desiredSubGridsPerDim : 1);
    }

  dax::Id3 cellsPerSubGrid = cellDims / subGridsPerDim;
  dax::Id3 remainderCellsPerSubGrids;
  for(std::size_t i=0; i < 3; ++i)
    {
    remainderCellsPerSubGrids[i] = cellDims[i] % subGridsPerDim[i];
    }

  for(std::size_t k=0; k < subGridsPerDim[2]; ++k)
    {
    for(std::size_t j=0; j < subGridsPerDim[1]; ++j)
      {
      for(std::size_t i=0; i < subGridsPerDim[0]; ++i)
        {
        const dax::Id3 ijk(i,j,k);

        //calculate out the remainder for this grid, and the offsets for
        //this subgrid at the same time
        dax::Id3 current_remainder(0,0,0);
        dax::Id3 subGridIJKOffset(0,0,0);
        for(std::size_t f=0; f < 3; ++f)
          {
          if(ijk[f] > 0)
            {
            subGridIJKOffset[f] = ijk[f] * cellsPerSubGrid[f];
            }
          if(ijk[f] == (subGridsPerDim[f]-1))
            {
            current_remainder[f] = remainderCellsPerSubGrids[f];
            }
          }

        dax::cont::UniformGrid< > subGrid;
        subGrid.SetSpacing( this->Spacing ); //same as the full grid

        //calculate the origin
        const dax::Vector3 offset = dax::make_Vector3(ijk[0] * cellsPerSubGrid[0],
                                                      ijk[1] * cellsPerSubGrid[1],
                                                      ijk[2] * cellsPerSubGrid[2]);
        const dax::Vector3 sub_origin = this->Origin + ( offset * this->Spacing);
        subGrid.SetOrigin( sub_origin );

        //calculate out the new extent
        dax::Extent3 sub_extent;
        sub_extent.Min = dax::Id3(0,0,0);
        sub_extent.Max = cellsPerSubGrid + current_remainder;
        subGrid.SetExtent(sub_extent);

        this->SubGrids.push_back(subGrid);
        this->SubGridCellIJKOffset.push_back( subGridIJKOffset );
        }
      }
    }

  //now create the rest of the vectors to the same size as the subgrids
  this->PerSubGridLowHighs.resize( this->SubGrids.size() );
  this->PerSubGridValues.resize( this->SubGrids.size(), NULL );

  logger << "Computed Sub Grids: " << timer.GetElapsedTime()
         << " sec ( " << data->GetNumberOfPoints() << " points )"
         << std::endl;
}

//----------------------------------------------------------------------------
SubdividedVolume::SubdividedVolume( const SubdividedVolume& other ):
  Origin(other.Origin),
  Spacing(other.Spacing),
  Extent(other.Extent),
  SubGrids(other.SubGrids),
  SubGridCellIJKOffset(other.SubGridCellIJKOffset),
  PerSubGridLowHighs(other.PerSubGridLowHighs),
  PerSubGridValues(other.PerSubGridValues)
{

}

//----------------------------------------------------------------------------
SubdividedVolume::~SubdividedVolume()
{
  //release all the data arrays
  const std::size_t size = this->numSubGrids();
  for(std::size_t i=0; i < size; ++i)
    {
    vtkDataArray* da = this->subGridValues(i);
    if(da)
      {
      da->Delete();
      }
    }
}


//----------------------------------------------------------------------------
SubdividedVolume& SubdividedVolume::operator= (SubdividedVolume other)
{
  std::swap(this->Origin,other.Origin);
  std::swap(this->Spacing,other.Spacing);
  std::swap(this->Extent,other.Extent);
  std::swap(this->SubGrids,other.SubGrids);
  std::swap(this->SubGridCellIJKOffset,other.SubGridCellIJKOffset);
  std::swap(this->PerSubGridLowHighs,other.PerSubGridLowHighs);
  std::swap(this->PerSubGridValues,other.PerSubGridValues);
  return *this;
}

//----------------------------------------------------------------------------
template<typename IteratorType, typename LoggerType>
void SubdividedVolume::ComputeHighLows(const IteratorType begin,
                                       const IteratorType end,
                                       LoggerType& logger)
{
  this->ComputePerSubGridValues(begin,end,logger);

  typedef typename std::iterator_traits<IteratorType>::value_type ValueType;

  dax::cont::Timer<> timer;

  //we parallelize this operation by stating that each worklet will
  //work on computing the low/high for an entire sub-grid
  typedef ::worklets::ComputeLowHighPerElement<ValueType> LowHighWorkletType;
  LowHighWorkletType computeLowHigh(this->PerSubGridValues);

  dax::cont::ArrayHandleCounting<dax::Id> countingHandle(0,this->numSubGrids());
  dax::cont::ArrayHandle< dax::Vector2 > lowHighResults;

  dax::cont::DispatcherMapField<LowHighWorkletType> dispatcher(computeLowHigh);
  dispatcher.Invoke( countingHandle, lowHighResults );

  //copy the results back from the lowHighResults into the stl vector
  //to do going to have to do this manually with a cast
  lowHighResults.CopyInto(this->PerSubGridLowHighs.begin());

  logger << "Computed Low High Field: " << timer.GetElapsedTime()
         << std::endl;
}

//----------------------------------------------------------------------------
template<typename IteratorType, typename LoggerType>
void SubdividedVolume::ComputePerSubGridValues(const IteratorType begin,
                                               const IteratorType end,
                                               LoggerType& logger)
{
  //find the default device adapter
  typedef DAX_DEFAULT_DEVICE_ADAPTER_TAG AdapterTag;

  //Make it easy to call the DeviceAdapter with the right tag
  typedef dax::cont::DeviceAdapterAlgorithm<AdapterTag> DeviceAdapter;

  dax::cont::Timer<> timer;

  typedef typename std::iterator_traits<IteratorType>::value_type ValueType;
  typedef std::vector< dax::cont::UniformGrid< > >::const_iterator gridIt;

  std::vector< vtkDataArray* >::iterator subGridValues = this->PerSubGridValues.begin();
  std::vector< dax::Id3 >::const_iterator ijkSubOffset = this->SubGridCellIJKOffset.begin();

  dax::cont::ArrayHandle<ValueType> fullGridValues =
        dax::cont::make_ArrayHandle(begin, std::distance(begin,end) );

  //abstract out the transform from full space to local space
  //once and call it to store the subgrid data once
  for(gridIt grid = this->SubGrids.begin();
      grid != this->SubGrids.end();
      ++grid, ++ijkSubOffset, ++subGridValues)
    {
    //allocate the contiguous space for the sub grid
    (*subGridValues) = make_vtkDataArray(ValueType());
    (*subGridValues)->SetNumberOfTuples((*grid).GetNumberOfPoints());
    (*subGridValues)->SetNumberOfComponents(1);

    //create temporary functor to fill continuous memory vector
    //so that we don't have to iterate over non continuous memory
    ::functors::SubGridValues<ValueType> functor(fullGridValues,
                                                 (*subGridValues),
                                                 this->getExtent(),
                                                 (*grid).GetExtent(),
                                                 (*ijkSubOffset));

    DeviceAdapter::Schedule(functor, (*grid).GetNumberOfPoints() );
    }

  logger << "Computed Explicit Values Per Sub Grid: " << timer.GetElapsedTime()
         << std::endl;
}


//----------------------------------------------------------------------------
template<typename ValueType, typename LoggerType>
dax::cont::UnstructuredGrid< dax::CellTagTriangle >
SubdividedVolume::ComputeSubGridContour(dax::Scalar isoValue,
                               std::size_t index,
                               ValueType,
                               LoggerType& logger)
{
  typedef  dax::cont::DispatcherGenerateInterpolatedCells<
                  ::worklets::ContourGenerate > InterpolatedDispatcher;

  typedef dax::cont::UnstructuredGrid< dax::CellTagTriangle >
                                                UnstructuredGridType;

  const ValueType* raw_values = reinterpret_cast<ValueType*>(
                            this->subGridValues(index)->GetVoidPointer(0));
  const dax::Id raw_values_len =
                            this->PerSubGridValues[index]->GetNumberOfTuples();

  //make an array handle the is read only reference to sub grid values
  dax::cont::ArrayHandle<ValueType> values =
    dax::cont::make_ArrayHandle(raw_values, raw_values_len);

  dax::cont::ArrayHandle<dax::Id> numTrianglesPerCell;

  dax::cont::DispatcherMapCell< ::worklets::ContourCount >
    classify( ( ::worklets::ContourCount(isoValue)) );
  classify.Invoke( this->subGrid(index), values, numTrianglesPerCell );

  //run the second step again with point merging
  InterpolatedDispatcher interpDispatcher( numTrianglesPerCell,
                              ::worklets::ContourGenerate(isoValue) );
  interpDispatcher.SetRemoveDuplicatePoints(true);
  UnstructuredGridType outGrid;
  interpDispatcher.Invoke(this->subGrid(index),
                          outGrid,
                          values);
  return outGrid;
}

//----------------------------------------------------------------------------
template<typename ValueType, typename LoggerType>
dax::cont::UnstructuredGrid< dax::CellTagTriangle >
SubdividedVolume::ContourSubGrid(dax::Scalar isoValue,
                               std::size_t index,
                               ValueType,
                               LoggerType& logger)
{
  return this->ComputeSubGridContour(isoValue,index,ValueType(),logger);
}

//----------------------------------------------------------------------------
template<typename ValueType, typename LoggerType>
dax::cont::UnstructuredGrid< dax::CellTagVertex,
  dax::cont::ArrayContainerControlTagImplicit<
      dax::cont::internal::ArrayPortalCounting<dax::Id> > >
SubdividedVolume::PointCloudSubGrid(dax::Scalar isoValue,
                                    std::size_t index,
                                    ValueType,
                                    LoggerType& logger)
{
  typedef dax::cont::UnstructuredGrid< dax::CellTagTriangle >
                                                UnstructuredGridType;

  typedef dax::cont::ArrayContainerControlTagImplicit<
      dax::cont::internal::ArrayPortalCounting<dax::Id> > CountingIdContainerType;
  typedef dax::cont::UnstructuredGrid< dax::CellTagVertex,
                                       CountingIdContainerType> PointCloudGridType;


  UnstructuredGridType triangleGrid =
                                 this->ComputeSubGridContour(isoValue,
                                                             index,
                                                             ValueType(),
                                                             logger);

  //make an unstructured grid with a virtual topology that start at zero
  //and monotonically increasing to the number of points we have
  PointCloudGridType outGrid(
    dax::cont::make_ArrayHandleCounting(0,
          triangleGrid.GetPointCoordinates().GetNumberOfValues()),
    triangleGrid.GetPointCoordinates());

  return outGrid;
}

//----------------------------------------------------------------------------
bool SubdividedVolume::isValidSubGrid(std::size_t index, dax::Scalar value)
{
  return this->PerSubGridLowHighs[index][0] <= value &&
         this->PerSubGridLowHighs[index][1] >= value;
}

}
}


/*


//----------------------------------------------------------------------------
template<typename ValueType, typename LoggerType>
void SubdividedVolume::Contour(dax::Scalar isoValue,
                               LoggerType& logger)
{
  logger << "Contour with value: " << isoValue << std::endl;

  dax::cont::Timer<> timer;
  dax::cont::Timer<> stageTimer;
  double stage1Time=0, stage2Time=0;

  typedef std::vector< dax::cont::UniformGrid< > >::const_iterator gridIt;

  //variables for info for the logger
  std::size_t numValidSubGrids=0;

  //run the first stage of the contour algorithm
  const std::size_t totalSubGrids = this->numSubGrids();

  typedef  dax::cont::DispatcherGenerateInterpolatedCells<
                  ::worklets::ContourGenerate > InterpolatedDispatcher;
  typedef dax::cont::UnstructuredGrid<
                  dax::CellTagTriangle > UnstructuredGridType;


  //store numTrianglePerCell to reduce the number of frees/news
  dax::cont::ArrayHandle<dax::Id> numTrianglesPerCell;

  //Store the second stage dispatcher so that we don't reallocate
  //the internal InterpolationWeights member variable for each iteration
  //this should also reduce the number of mallocs
  InterpolatedDispatcher interpDispatcher( numTrianglesPerCell,
                              ::worklets::ContourGenerate(isoValue) );

  interpDispatcher.SetReleaseCount(false);
  interpDispatcher.SetRemoveDuplicatePoints(true);

  for(std::size_t i = 0; i < totalSubGrids; ++i)
    {
    if(this->isValidSubGrid(i, isoValue))
      {
      const ValueType* raw_values = reinterpret_cast<ValueType*>(this->subGridValues(i)->GetVoidPointer(0));
      const dax::Id raw_values_len = this->PerSubGridValues[i]->GetNumberOfTuples();

      stageTimer.Reset();
      //make an array handle the is read only reference to sub grid values
      dax::cont::ArrayHandle<ValueType> values =
        dax::cont::make_ArrayHandle(raw_values, raw_values_len);

      dax::cont::DispatcherMapCell< ::worklets::ContourCount >
        classify( ( ::worklets::ContourCount(isoValue)) );
      classify.Invoke( this->subGrid(i), values, numTrianglesPerCell );
      stage1Time += stageTimer.GetElapsedTime();

      stageTimer.Reset();
      //run the second step again with point merging

      UnstructuredGridType secondOutGrid;
      interpDispatcher.Invoke(this->subGrid(i),
                              secondOutGrid,
                              values);
      stage2Time += stageTimer.GetElapsedTime();



      ++numValidSubGrids;
      }
    }

  logger << "contour: " << timer.GetElapsedTime()  << " sec" << std::endl;
  logger << "stage 1: " << stage1Time  << " sec" << std::endl;
  logger << "stage 2: " << stage2Time  << " sec" << std::endl;
  logger << numValidSubGrids << "(" << (numValidSubGrids/(float)totalSubGrids * 100)
         << "%) of the subgrids are valid " << std::endl;
}


//----------------------------------------------------------------------------
template<typename ValueType, typename LoggerType>
void SubdividedVolume::Contour(dax::Scalar isoValue,
                               LoggerType& logger)
{
  dax::cont::Timer<> timer;

  typedef std::vector< dax::cont::UniformGrid< > >::const_iterator gridIt;


  //run the first stage of the contour algorithm
  const std::size_t totalSubGrids = this->numSubGrids();

  std::vector<dax::Id> validSubGrids;
  for(std::size_t i = 0; i < totalSubGrids; ++i)
    {
    if(this->isValidSubGrid(i, isoValue))
      {
      validSubGrids.push_back(i);
      }
    }

  const std::size_t numValidSubGrids=validSubGrids.size();

  typedef ::worklets::ComputeVtkContour  ContourWorkletType;

  //make the functor to run the vtk contour algorithm
  ContourWorkletType computeContour(isoValue, this->SubGrids,
                                    this->PerSubGridValues);
  dax::cont::DispatcherMapField<ContourWorkletType> dispatcher(computeContour);
  dispatcher.Invoke( dax::cont::make_ArrayHandle(validSubGrids) );

  logger << "contour: " << timer.GetElapsedTime()  << " sec" << std::endl;
  logger << numValidSubGrids << "(" << (numValidSubGrids/(float)totalSubGrids * 100)
         << "%) of the subgrids are valid " << std::endl;
}
*/