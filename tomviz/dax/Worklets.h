/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#ifndef tomvizWorklets_h
#define tomvizWorklets_h

#include <dax/cont/arg/ExecutionObject.h>
#include <dax/cont/ArrayHandle.h>

#include <dax/math/Compare.h>
#include <dax/exec/WorkletMapField.h>
#include <dax/worklet/MarchingCubes.h>

#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkContourFilter.h"

//If we are building against a version of dax that doesn't support windows
//we will manually set these macros. This will allow us to still compile properly
//on Linux and OSX, it is doubtful though that we will build properly on windows
#ifndef FieldIn
# define FieldIn dax::cont::arg::Field(*)(In)
# define FieldOut dax::cont::arg::Field(*)(Out)
# define FieldPointIn dax::cont::arg::Field(*)(In,Point)
# define FieldCellIn dax::cont::arg::Field(*)(In,Cell)
# define FieldCellOut dax::cont::arg::Field(*)(Out,Cell)
# define TopologyIn dax::cont::arg::Topology(*)(In)
# define TopologyOut dax::cont::arg::Topology(*)(Out)
# define GeometryIn dax::cont::arg::Geometry(*)(In)
# define GeometryOut dax::cont::arg::Geometry(*)(Out)
# define AsVertices(pos) dax::cont::arg::Topology::Vertices(*)(pos)
#endif

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
                  const dax::Extent3& subGridExtent,
                  dax::Id3 subGridsOffsetsInFullGrid):
    InputPortal(fullGridContourValues.PrepareForInput()),
    OutputPortal(reinterpret_cast<ValueType*>(outputArray->GetVoidPointer(0))),
    FullExtent(fullExtent),
    SubGridExtent(subGridExtent),
    FullGridCellIJKOffset(subGridsOffsetsInFullGrid)
    {
    }

    DAX_EXEC_EXPORT
    void operator()(dax::Id pointIndex) const
    {
      //compute the local point ijk index
      const dax::Id3 local_ijk = dax::flatIndexToIndex3(pointIndex, this->SubGridExtent);

      //the problem is that we have our grids cell offset, and not our grids
      //point offset into the full grid
      const dax::Id3 global_ijk = local_ijk + this->FullGridCellIJKOffset;

      //we can't use dax here as index3ToFlatIndex doesn't support negative
      //extents
      const dax::Id3 dims = dax::extentDimensions(this->FullExtent);
      const dax::Id index = global_ijk[0] + dims[0]*(global_ijk[1] + dims[1]*global_ijk[2]);

      this->OutputPortal[pointIndex] = this->InputPortal.Get( index );
    }

    DAX_CONT_EXPORT
    void SetErrorMessageBuffer(const dax::exec::internal::ErrorMessageBuffer &)
    {  }

  typename dax::cont::ArrayHandle<ValueType>::PortalConstExecution InputPortal;
  ValueType* OutputPortal;
  dax::Extent3 FullExtent;
  dax::Extent3 SubGridExtent;
  dax::Id3 FullGridCellIJKOffset;
  };
}

namespace worklets
{

//CPU only worklet, would not work on GPU, would require a rewrite to
//parallelize the computation of the low high into a map/reduce pair of worklets
template<typename ValueType>
struct ComputeLowHighPerElement : dax::exec::WorkletMapField
{
  typedef void ControlSignature(FieldIn,FieldOut);
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
    const ValueType* rawValues = reinterpret_cast<const ValueType*>(
                                     this->Values[index]->GetVoidPointer(0));

    dax::Tuple<ValueType,2> lh;
    lh[0] = rawValues[0];
    lh[1] = rawValues[0];
    for(dax::Id i=1; i < size; ++i)
      {
      lh[0] = std::min(lh[0],rawValues[i]);
      lh[1] = std::max(lh[1],rawValues[i]);
      }
    return dax::make_Vector2(lh[0],lh[1]);
  }
  std::vector< vtkDataArray* > Values;
};

// -----------------------------------------------------------------------------
class ContourCount : public dax::exec::WorkletMapCell
{
public:
  typedef void ControlSignature(TopologyIn, FieldPointIn, FieldCellOut);
  typedef _3 ExecutionSignature(_2);

  DAX_CONT_EXPORT ContourCount(dax::Scalar isoValue)
    : IsoValue(isoValue) {  }

  template<class CellTag, class ValueType>
  DAX_EXEC_EXPORT
  dax::Id operator()(
      const dax::exec::CellField<ValueType,CellTag> &values) const
  {
    // If you get a compile error on the following line, it means that this
    // worklet was used with an improper cell type.  Check the cell type for the
    // input grid given in the control environment.
    return this->GetNumFaces(
          values,
          typename dax::CellTraits<CellTag>::CanonicalCellTag());
  }
private:
  dax::Scalar IsoValue;

  template<class CellTag, class ValueType>
  DAX_EXEC_EXPORT
  dax::Id GetNumFaces(const dax::exec::CellField<ValueType,CellTag> &values,
                      dax::CellTagHexahedron) const
  {
    const int voxelClass =
    dax::worklet::internal::marchingcubes::GetHexahedronClassification(IsoValue,values);
    return dax::worklet::internal::marchingcubes::NumFaces[voxelClass];
  }
};


// -----------------------------------------------------------------------------
class ContourGenerate : public dax::exec::WorkletInterpolatedCell
{
public:

  typedef void ControlSignature(TopologyIn, GeometryOut, FieldPointIn);
  typedef void ExecutionSignature(AsVertices(_1), _2, _3, VisitIndex);

  DAX_CONT_EXPORT ContourGenerate(dax::Scalar isoValue)
    : IsoValue(isoValue){ }

  template<class CellTag, class ValueType>
  DAX_EXEC_EXPORT void operator()(
      const dax::exec::CellVertices<CellTag>& verts,
      dax::exec::InterpolatedCellPoints<dax::CellTagTriangle>& outCell,
      const dax::exec::CellField<ValueType,CellTag> &values,
      dax::Id inputCellVisitIndex) const
  {
    // If you get a compile error on the following line, it means that this
    // worklet was used with an improper cell type.  Check the cell type for the
    // input grid given in the control environment.
    this->BuildTriangle(
          verts,
          outCell,
          values,
          inputCellVisitIndex,
          typename dax::CellTraits<CellTag>::CanonicalCellTag());
  }

private:
  dax::Scalar IsoValue;

  template<class CellTag, class ValueType>
  DAX_EXEC_EXPORT void BuildTriangle(
      const dax::exec::CellVertices<CellTag>& verts,
      dax::exec::InterpolatedCellPoints<dax::CellTagTriangle>& outCell,
      const dax::exec::CellField<ValueType,CellTag> &values,
      dax::Id inputCellVisitIndex,
      dax::CellTagHexahedron) const
  {
    using dax::worklet::internal::marchingcubes::TriTable;
    // These should probably be available through the voxel class
    const unsigned char voxelVertEdges[12][2] ={
        {0,1}, {1,2}, {3,2}, {0,3},
        {4,5}, {5,6}, {7,6}, {4,7},
        {0,4}, {1,5}, {2,6}, {3,7},
      };

    const int voxelClass =
        dax::worklet::internal::marchingcubes::GetHexahedronClassification(IsoValue,values);

    //save the point ids and ratio to interpolate the points of the new cell
    for (dax::Id outVertIndex = 0;
         outVertIndex < outCell.NUM_VERTICES;
         ++outVertIndex)
      {
      const unsigned char edge = TriTable[voxelClass][(inputCellVisitIndex*3)+outVertIndex];
      const int vertA = voxelVertEdges[edge][0];
      const int vertB = voxelVertEdges[edge][1];

      // Find the weight for linear interpolation
      const dax::Scalar weight = (IsoValue - values[vertA]) /
                                (values[vertB]-values[vertA]);

      outCell.SetInterpolationPoint(outVertIndex,
                                    verts[vertA],
                                    verts[vertB],
                                    weight);
      }
  }
};


}

/*

//CPU only worklet, would not work on GPU, would require a rewrite to
//parallelize the computation of the low high into a map/reduce pair of worklets
struct ComputeVtkContour : dax::exec::WorkletMapField
{
  typedef void ControlSignature(Field(In));
  typedef void ExecutionSignature(_1);

  DAX_CONT_EXPORT
  ComputeVtkContour(dax::Scalar isoValue,
                    const std::vector<dax::cont::UniformGrid< > >& grids,
                    const std::vector< vtkDataArray* >& values):
    IsoValue(isoValue),
    Grids(grids),
    Values(values)
  {
  }

  DAX_EXEC_EXPORT
  void operator()(dax::Id i) const
  {
    vtkNew<vtkContourFilter> vContour;
    vContour->SetValue(0, this->IsoValue);
    vContour->ComputeGradientsOff();
    vContour->ComputeScalarsOff();
    vtkNew<vtkImageData> vImage;

    //use dax to call VTK for each subgrid
    vImage->SetOrigin( this->Grids[i].GetOrigin()[0],
                       this->Grids[i].GetOrigin()[1],
                       this->Grids[i].GetOrigin()[2] );
    vImage->SetSpacing( this->Grids[i].GetSpacing()[0],
                        this->Grids[i].GetSpacing()[1],
                        this->Grids[i].GetSpacing()[2] );
    vImage->SetExtent( 0,
                       this->Grids[i].GetExtent().Max[0],
                       0,
                       this->Grids[i].GetExtent().Max[1],
                       0,
                       this->Grids[i].GetExtent().Max[2]);
    this->Values[i]->SetName("ISOValues");
    vImage->GetPointData()->SetScalars( this->Values[i] );
    vImage->GetPointData()->SetActiveScalars( "ISOValues" );

    vContour->SetInputData(vImage.GetPointer());
    vContour->Update();
  }

  dax::Scalar IsoValue;
  std::vector<dax::cont::UniformGrid< > > Grids;
  std::vector< vtkDataArray* > Values;
};
*/

#endif
