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
#ifndef __SubdividedVolume_h
#define __SubdividedVolume_h

#include <dax/Types.h>
#include <dax/cont/UniformGrid.h>
#include <dax/cont/UnstructuredGrid.h>

#include "vtkDataArray.h"
#include "vtkSmartPointer.h"
#include "vtkPolyData.h"
#include <vector>

namespace TEM
{
namespace accel
{
  class SubdividedVolume
  {
  public:
  SubdividedVolume() { }

  template< typename ImageDataType, typename LoggerType >
  inline SubdividedVolume( std::size_t subGridsPerDim,
                           ImageDataType* data,
                           LoggerType& logger );

  template<typename IteratorType, typename LoggerType>
  inline void ComputeHighLows(IteratorType begin, IteratorType end, LoggerType& logger);

  inline bool isValidSubGrid(std::size_t index, dax::Scalar value);

  template<typename ValueType, typename LoggerType>
  inline vtkSmartPointer< vtkPolyData >
  ContourSubGrid(dax::Scalar isoValue, std::size_t index, ValueType, LoggerType& logger);

  template<typename ValueType, typename LoggerType>
  inline vtkSmartPointer< vtkPolyData >
  PointCloudSubGrid(dax::Scalar isoValue, std::size_t index, ValueType, LoggerType& logger);

  const dax::cont::UniformGrid< >& subGrid( std::size_t index ) const
    { return SubGrids[index]; }

  vtkDataArray* subGridValues( std::size_t index ) const
    { return PerSubGridValues[index]; }

  void ReleaseAllResources()
    {
    PerSubGridLowHighs.clear();
    SubGrids.clear();
    }

  std::size_t numSubGrids() const { return SubGrids.size(); }

  dax::Vector3 getOrigin() const { return Origin; }
  dax::Vector3 getSpacing() const { return Spacing; }
  dax::Extent3 getExtent() const { return Extent; }

private:
  template<typename IteratorType, typename LoggerType>
  void ComputePerSubGridValues(IteratorType begin, IteratorType end, LoggerType& logger);

  template<typename ValueType, typename LoggerType>
  inline dax::cont::UnstructuredGrid<dax::CellTagTriangle>
  ComputeSubGridContour(dax::Scalar isoValue, std::size_t index, ValueType, LoggerType& logger);

  dax::Vector3 Origin;
  dax::Vector3 Spacing;
  dax::Extent3 Extent;

  std::vector< dax::cont::UniformGrid< > > SubGrids;
  std::vector< dax::Id3 > SubGridCellIJKOffset; //offsets

  //store PerSubGridLowHighs as 2 floats, as that is the maximum data size
  //that we support
  std::vector< dax::Vector2 > PerSubGridLowHighs;

  //store the sub grids cached values as vtkDataArray, and uses the
  //ValueType of the method for quick casting of all the vtkDataArray's
  //to the correct type
  std::vector< vtkDataArray* > PerSubGridValues;
  };

//helper functors to generalize calling Contour or Threshold based on
//already having a SubdividedVolume
struct ContourFunctor
{
  TEM::accel::SubdividedVolume& Volume;

  ContourFunctor(TEM::accel::SubdividedVolume& v):Volume(v){}

  template<typename ValueType, typename LoggerType>
  vtkSmartPointer< vtkPolyData> operator()(double v, std::size_t i,
                                           ValueType, LoggerType& logger)
  {
    return this->Volume.ContourSubGrid(v,i,ValueType(),logger);
  }
};

struct ThresholdFunctor
{
  TEM::accel::SubdividedVolume& Volume;

  ThresholdFunctor(TEM::accel::SubdividedVolume& v):Volume(v){}

  template<typename ValueType, typename LoggerType>
  vtkSmartPointer< vtkPolyData> operator()(double v, std::size_t i,
                                           ValueType, LoggerType& logger)
  {
    return this->Volume.PointCloudSubGrid(v,i,ValueType(),logger);
  }
};

}
}

#include "SubdividedVolume.txx"
#endif