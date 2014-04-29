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
#include <dax/cont/ArrayHandle.h>


#include <dax/math/Compare.h>
#include <dax/exec/WorkletMapField.h>


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

namespace worklets
{

//CPU only worklet, would not work on GPU, would require a rewrite to
//parallelize the computation of the low high into a map/reduce pair of worklets
template<typename ValueType>
struct ComputeLowHighPerElement : dax::exec::WorkletMapField
{
  typedef void ControlSignature(Field(In),Field(Out));
  typedef _2 ExecutionSignature(_1);

  ComputeLowHighPerElement(const std::vector< vtkDataArray* >& values):
    Values(values)
  {
  }

  dax::Tuple<ValueType,2> operator()(dax::Id index) const
  {
    const dax::Id size = this->Values[index]->GetNumberOfTuples();
    const ValueType* rawValues = static_cast<const ValueType*>(
                                     this->Values[index]->GetVoidPointer(0));

    dax::Tuple<ValueType,2> lh;
    lh[0] = rawValues[0];
    lh[1] = rawValues[0];
    for(dax::Id i=1; i < size; ++i)
      {
      lh[0] = std::min(rawValues[i],lh[0]);
      lh[1] = std::max(rawValues[i],lh[1]);
      }
    return lh;
  }
  const std::vector< vtkDataArray* >& Values;
};

}
