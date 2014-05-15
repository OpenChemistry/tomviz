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

//supported iso values array types
#include "vtkDataSetAttributes.h"
#include "vtkCharArray.h"
#include "vtkFloatArray.h"
#include "vtkIntArray.h"
#include "vtkShortArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkUnsignedCharArray.h"

#include "vtkContourFilter.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkSynchronizedTemplates3D.h"

#include "Worklets.h"

namespace
{
  vtkDataArray* make_vtkDataArray(float)
    { return vtkFloatArray::New(); }

  vtkDataArray* make_vtkDataArray(int)
    { return vtkIntArray::New(); }
  vtkDataArray* make_vtkDataArray(unsigned int)
    { return vtkUnsignedIntArray::New(); }

  vtkDataArray* make_vtkDataArray(short)
    { return vtkShortArray::New(); }
  vtkDataArray* make_vtkDataArray(unsigned short)
    { return vtkUnsignedShortArray::New(); }

  vtkDataArray* make_vtkDataArray(char)
    { return vtkCharArray::New(); }
  vtkDataArray* make_vtkDataArray(unsigned char)
    { return vtkUnsignedCharArray::New(); }
}

namespace TEM
{
namespace accel
{

//----------------------------------------------------------------------------
template< typename ImageDataType, typename LoggerType >
SubdividedVolume::SubdividedVolume( std::size_t subGridsPerDim,
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

  dax::Id3 size = dax::extentDimensions(this->Extent);

  dax::Id3 size_per_sub_grid, remainder;
  for(std::size_t dim=0; dim < 3; ++dim)
    {
    size_per_sub_grid[dim] = size[dim] / subGridsPerDim;
    remainder[dim] = size[dim] % subGridsPerDim;
    }

  dax::Id3 my_remainder;

  for(std::size_t k=0; k < subGridsPerDim; ++k)
    {
    if(k == subGridsPerDim-1)
      { my_remainder[2]=remainder[2]; }
    else { my_remainder[2]=0; }

    for(std::size_t j=0; j < subGridsPerDim; ++j)
      {
      if(j == subGridsPerDim-1)
        { my_remainder[1]=remainder[1]; }
      else { my_remainder[1]=0; }

      for(std::size_t i=0; i < subGridsPerDim; ++i)
        {
        if(i == subGridsPerDim-1)
          { my_remainder[0]=remainder[0]; }
        else { my_remainder[0]=0; }

        dax::cont::UniformGrid< > subGrid;
        subGrid.SetSpacing( this->Spacing ); //same as the full grid

        //calculate the origin
        dax::Vector3 offset(i * size_per_sub_grid[0],
                            j * size_per_sub_grid[1],
                            k * size_per_sub_grid[2]);
        dax::Vector3 sub_origin = this->Origin + ( offset * this->Spacing);

        sub_origin[0] +=  my_remainder[0];
        sub_origin[1] +=  my_remainder[1];
        sub_origin[2] +=  my_remainder[2];
        subGrid.SetOrigin( sub_origin );

        //calculate out the new extent
        dax::Extent3 sub_extent;
        sub_extent.Min = dax::Id3(0,0,0);
        sub_extent.Max = size_per_sub_grid - dax::make_Id3(1,1,1) + my_remainder; //account for it being cells
        subGrid.SetExtent(sub_extent);

        this->SubGrids.push_back(subGrid);

        //I need to verify that this should be point based, instead of cell
        //based
        dax::Id3 worldIJKOffset(i,j,k);
        worldIJKOffset = (worldIJKOffset * size_per_sub_grid) + my_remainder;
        this->SubGridsIJKOffset.push_back( worldIJKOffset );
        }
      }
    }
  //now create the rest of the vectors to the same size as the subgrids
  this->PerSubGridLowHighs.resize( this->SubGrids.size() );
  this->PerSubGridValues.resize( this->SubGrids.size() );

  logger << "Computed Sub Grids: " << timer.GetElapsedTime()
         << " sec ( " << data->GetNumberOfPoints() << " points )"
         << std::endl;
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
  std::vector< dax::Id3 >::const_iterator ijkSubOffset = this->SubGridsIJKOffset.begin();

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
                                                 (*ijkSubOffset));

    //We use the low level id3 scheduler to generate each sub grids values
    //this is more efficient as we don't need to compute the local ijk
    DeviceAdapter::Schedule(functor,
                            dax::extentDimensions((*grid).GetExtent()));
    }

  logger << "Computed Explicit Values Per Sub Grid: " << timer.GetElapsedTime()
         << std::endl;
}

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
dax::cont::UnstructuredGrid< dax::CellTagTriangle >
SubdividedVolume::ContourSubGrid(dax::Scalar isoValue,
                               std::size_t index,
                               ValueType,
                               LoggerType& logger)
{
  typedef  dax::cont::DispatcherGenerateInterpolatedCells<
                  ::worklets::ContourGenerate > InterpolatedDispatcher;

  typedef dax::cont::UnstructuredGrid<
                  dax::CellTagTriangle > UnstructuredGridType;

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
/*

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

//----------------------------------------------------------------------------
bool SubdividedVolume::isValidSubGrid(std::size_t index, dax::Scalar value)
{
  return this->PerSubGridLowHighs[index][0] <= value &&
         this->PerSubGridLowHighs[index][1] >= value;
}


}
}