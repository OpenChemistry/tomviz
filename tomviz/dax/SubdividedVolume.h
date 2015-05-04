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

#include "DataSetConverters.h"

#include <vector>

namespace TEM
{
namespace accel
{
  class SubdividedVolume
  {
  typedef dax::cont::UnstructuredGrid< dax::CellTagTriangle >
          ContourGridReturnType;

  typedef dax::cont::ArrayContainerControlTagImplicit<
      dax::cont::internal::ArrayPortalCounting<dax::Id> > CountingIdContainerType;
  typedef dax::cont::UnstructuredGrid< dax::CellTagVertex,
                                       CountingIdContainerType>
          PointCloudSubGridReturnType;

public:

  SubdividedVolume();

  template< typename ImageDataType, typename LoggerType >
  inline SubdividedVolume( std::size_t subGridsPerDim,
                           ImageDataType* data,
                           LoggerType& logger );

  SubdividedVolume( const SubdividedVolume& other);

  ~SubdividedVolume();
  SubdividedVolume& operator= (SubdividedVolume other);

  template<typename IteratorType, typename LoggerType>
  inline void ComputeHighLows(IteratorType begin, IteratorType end, LoggerType& logger);

  inline bool isValidSubGrid(std::size_t index, dax::Scalar value);

  template<typename ValueType, typename LoggerType>
  ContourGridReturnType
  ContourSubGrid(dax::Scalar isoValue, std::size_t index, ValueType, LoggerType& logger);

  template<typename ValueType, typename LoggerType>
  PointCloudSubGridReturnType
  PointCloudSubGrid(dax::Scalar isoValue, std::size_t index, ValueType, LoggerType& logger);

  const dax::cont::UniformGrid< >& subGrid( std::size_t index ) const
    { return SubGrids[index]; }

  vtkDataArray* subGridValues( std::size_t index ) const
    { return PerSubGridValues[index]; }


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
  typedef dax::cont::UnstructuredGrid< dax::CellTagTriangle > ReturnType;


  ContourFunctor(TEM::accel::SubdividedVolume& v):Volume(v){}

  template<typename ValueType, typename LoggerType>
  ReturnType operator()(double v, std::size_t i, ValueType, LoggerType& logger)
  {
    return this->Volume.ContourSubGrid(v,i,ValueType(),logger);
  }

  TEM::accel::SubdividedVolume& Volume;
};

struct PointCloudFunctor
{
  typedef dax::cont::ArrayContainerControlTagImplicit<
      dax::cont::internal::ArrayPortalCounting<dax::Id> > CountingIdContainerType;
  typedef dax::cont::UnstructuredGrid< dax::CellTagVertex,
                                       CountingIdContainerType> ReturnType;


  PointCloudFunctor(TEM::accel::SubdividedVolume& v):Volume(v){}

  template<typename ValueType, typename LoggerType>
  ReturnType operator()(double v, std::size_t i, ValueType, LoggerType& logger)
  {
    return this->Volume.PointCloudSubGrid(v,i,ValueType(),logger);
  }

  TEM::accel::SubdividedVolume& Volume;
};

}
}

#include "SubdividedVolume.txx"
#endif