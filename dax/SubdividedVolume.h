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
#include <dax/cont/ArrayHandle.h>
#include <dax/cont/UniformGrid.h>

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
  SubdividedVolume( std::size_t subGridsPerDim,
                    ImageDataType* data,
                    LoggerType& logger );

  template<typename IteratorType, typename LoggerType>
  void ComputeHighLows(IteratorType begin, IteratorType end, LoggerType& logger);

  bool isValidSubGrid(std::size_t index, dax::Scalar value);

  const dax::cont::UniformGrid< >& subGrid( std::size_t index ) const
    { return SubGrids[index]; }

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
  dax::Vector3 Origin;
  dax::Vector3 Spacing;
  dax::Extent3 Extent;

  //store PerSubGridLowHighs as 2 floats, as that is the maximum data size
  //that we support
  std::vector< dax::Vector2 > PerSubGridLowHighs;
  std::vector< dax::Id3 > SubGridsIJKOffset; //offsets
  std::vector< dax::cont::UniformGrid< > > SubGrids;
  };
}
}

#include "SubdividedVolume.txx"
#endif