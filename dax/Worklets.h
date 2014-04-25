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

#include <dax/cont/arg/ExecutionObject.h>
#include <dax/cont/DeviceAdapter.h>
#include <dax/cont/ArrayHandle.h>
#include <dax/cont/UniformGrid.h>
#include <dax/cont/UnstructuredGrid.h>

#include <dax/math/Compare.h>

#include <dax/exec/WorkletMapCell.h>
#include <dax/worklet/MarchingCubes.h>


namespace functors
{
  template<typename ValueType>
  struct SubGridValues
  {
    SubGridValues(){}

    SubGridValues(dax::cont::ArrayHandle<ValueType> fullGridContourValues,
                  const dax::Extent3& subExtent,
                  const dax::Extent3& fullExtent,
                  dax::Id3 subGridsOffsetsInFullGrid):
    Portal(fullGridContourValues.PrepareForInput()),
    SubExtent(subExtent),
    FullExtent(fullExtent),
    FullGridIJKOffset(subGridsOffsetsInFullGrid)
    {

    }

    ValueType operator()(dax::Id index) const
    {
      dax::Id3 local_ijk = dax::flatIndexToIndex3(index, this->SubExtent);
      dax::Id3 global_ijk = local_ijk + this->FullGridIJKOffset;
      return this->Portal.Get(dax::index3ToFlatIndex(global_ijk, this->FullExtent));
      // return this->Portal.Get(index)
    }
  typename dax::cont::ArrayHandle<ValueType>::PortalConstExecution Portal;
  dax::Extent3 SubExtent;
  dax::Extent3 FullExtent;
  dax::Id3 FullGridIJKOffset;
  };

}

namespace worklet
{

class FindLowHigh :  public dax::exec::WorkletMapCell
{
public:
  typedef void ControlSignature(Topology, Field(Point), Field(Out));
  typedef _3 ExecutionSignature(_2);

  template<class ValueType, class CellTag>
  DAX_EXEC_EXPORT
  dax::Tuple<ValueType,2> operator()(
    const dax::exec::CellField<ValueType,CellTag> &values) const
  {
    dax::Tuple<ValueType,2> lh;
    lh[0] = values[0];
    lh[1] = values[1];
    for(int i=1; i < 8; ++i)
      {
      lh[0] = dax::math::Min(values[i],lh[0]);
      lh[1] = dax::math::Max(values[i], lh[1]);
      }
    return lh;
  }
};

}
