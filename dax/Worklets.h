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
    DAX_CONT_EXPORT
    SubGridValues(){}

    DAX_CONT_EXPORT
    SubGridValues(dax::cont::ArrayHandle<ValueType> fullGridContourValues,
                  vtkDataArray* outputArray,
                  const dax::Extent3& fullExtent,
                  dax::Id3 subGridsOffsetsInFullGrid):
    InputPortal(fullGridContourValues.PrepareForInput()),
    OutputPortal(static_cast<ValueType*>(outputArray->GetVoidPointer(0))),
    FullExtent(fullExtent),
    FullGridIJKOffset(subGridsOffsetsInFullGrid),
    FullGridDims(dax::extentDimensions(fullExtent))
    {

    }

    DAX_EXEC_EXPORT
    void operator()(dax::exec::internal::IJKIndex local_ijk) const
    {
      const dax::Id3 global_ijk = local_ijk.GetIJK() + this->FullGridIJKOffset;

      const dax::Id3 deltas(global_ijk[0] - this->FullExtent.Min[0],
                            global_ijk[1] - this->FullExtent.Min[1],
                            global_ijk[2] - this->FullExtent.Min[2]);
      const dax::Id index = deltas[0] + this->FullGridDims[0]*(deltas[1] + this->FullGridDims[1]*deltas[2]);

      this->OutputPortal[local_ijk] = this->InputPortal.Get( index );
    }

    DAX_CONT_EXPORT
    void SetErrorMessageBuffer(const dax::exec::internal::ErrorMessageBuffer &)
    {  }

  typename dax::cont::ArrayHandle<ValueType>::PortalConstExecution InputPortal;
  ValueType* OutputPortal;
  dax::Extent3 FullExtent;
  dax::Id3 FullGridIJKOffset;
  dax::Id3 FullGridDims;
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

  DAX_CONT_EXPORT
  ComputeLowHighPerElement(const std::vector< vtkDataArray* >& values):
    Values(values)
  {
  }

  DAX_EXEC_EXPORT
  dax::Vector2 operator()(dax::Id index) const
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
