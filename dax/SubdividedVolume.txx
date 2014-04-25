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
#include <dax/cont/Timer.h>
#include <dax/cont/DispatcherMapField.h>
#include <dax/cont/DispatcherMapCell.h>

#include "Worklets.h"

namespace TEM
{
namespace accel
{

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
        this->SubGridsIJKOffset.push_back( dax::Id3(i,j,k) );
        }
      }
    }
  //now create the rest of the vectors to the same size as the subgrids
  this->PerSubGridLowHighs.resize( this->SubGrids.size() );

  logger << "Computed Sub Grids: " << timer.GetElapsedTime()
         << " sec ( " << data->GetNumberOfPoints() << " points)"
         << std::endl;
}

template<typename IteratorType, typename LoggerType>
void SubdividedVolume::ComputeHighLows(const IteratorType begin,
                                       const IteratorType end,
                                       LoggerType& logger)
{
  typedef typename std::iterator_traits<IteratorType>::value_type ValueType;

  dax::cont::Timer<> timer;
  dax::cont::Timer<> worklet_timer;
  double workletTotalTime=0;

  dax::Id numPoints = 0;

  typedef std::vector< dax::cont::UniformGrid< > >::const_iterator gridIt;


  std::vector< dax::Id3 >::const_iterator ijkSubOffset = this->SubGridsIJKOffset.begin();
  std::vector< dax::Vector2 >::iterator lowHighPerSub = this->PerSubGridLowHighs.begin();

  //reuse lowhigh on each iteration
  dax::cont::ArrayHandle< dax::Tuple<ValueType,2> > lowhigh;

  //in the future if we want to support cuda, we need to upload only a subset
  //of the data instead of everything, else we will run out of memory on cuda
  //devices, plus this adds some override to keep converting from sub-grid
  //space to full grid space
  dax::cont::ArrayHandle<ValueType> fullGridValues =
        dax::cont::make_ArrayHandle(begin, std::distance(begin,end) );

  for(gridIt grid = this->SubGrids.begin();
      grid != this->SubGrids.end();
      ++grid, ++ijkSubOffset, ++lowHighPerSub)
    {
    dax::cont::UniformGrid< > g = *grid;
    numPoints += g.GetNumberOfPoints();

    typedef dax::cont::ArrayHandleImplicit< ValueType,
              ::functors::SubGridValues<ValueType> > PointsValuesForGrid;

    worklet_timer.Reset();
    ::functors::SubGridValues<ValueType> functor(fullGridValues,
                                                 g.GetExtent(),
                                                 this->getExtent(),
                                                 (*ijkSubOffset));

    PointsValuesForGrid subgridValues(functor, g.GetNumberOfPoints());

    //compute the low highs for each sub grid
    dax::cont::DispatcherMapCell< ::worklet::FindLowHigh >().Invoke(
                                                g, subgridValues, lowhigh);
    workletTotalTime += worklet_timer.GetElapsedTime();

    if(g.GetNumberOfCells() > 0)
      {
      const dax::Id size = lowhigh.GetNumberOfValues();
      dax::Tuple<ValueType,2> lh;
      lh[0] = lowhigh.GetPortalConstControl().Get(0)[0];
      lh[1] = lowhigh.GetPortalConstControl().Get(0)[1];
      for(dax::Id i=1; i < size; ++i)
        {
        dax::Tuple<ValueType,2> v = lowhigh.GetPortalConstControl().Get(i);
        lh[0] = std::min(v[0],lh[0]);
        lh[1] = std::max(v[1],lh[1]);
        }
      (*lowHighPerSub) = dax::Vector2(lh[0],lh[1]);
      }
    else
      {
      //invalid cell so make the low high impossible to match
      (*lowHighPerSub) = dax::Vector2(1,-1);
      }
    }

  logger << "Computed Low High Field: " << timer.GetElapsedTime()
         << " sec (" << numPoints << " points)"
         << std::endl;
  logger << "Worklet time was: " << workletTotalTime
         << " ( " << (workletTotalTime / timer.GetElapsedTime())*100 << "% ) "
         << std::endl;

}

bool SubdividedVolume::isValidSubGrid(std::size_t index, dax::Scalar value)
  {
  return this->PerSubGridLowHighs[index][0] <= value &&
         this->PerSubGridLowHighs[index][1] >= value;
  }


}
}