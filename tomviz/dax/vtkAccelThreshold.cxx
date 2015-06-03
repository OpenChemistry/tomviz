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

#include "vtkAccelThreshold.h"

#include "vtkDataArray.h"
#include "vtkDataArrayTemplate.h"
#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkUnstructuredGrid.h"
#include "vtkNew.h"

#include <dax/cont/DeviceAdapter.h>
#include <dax/cont/ArrayHandle.h>
#include <dax/cont/DispatcherGenerateTopology.h>
#include <dax/cont/DispatcherMapCell.h>
#include <dax/cont/UniformGrid.h>
#include <dax/worklet/Threshold.h>

#include "DataSetConverters.h"

#include <iostream>



namespace
{
#define temTemplateMacro(call)                                              \
  vtkTemplateMacroCase(VTK_FLOAT, float, call);                             \
  vtkTemplateMacroCase(VTK_INT, int, call);                                 \
  vtkTemplateMacroCase(VTK_UNSIGNED_INT, unsigned int, call);               \
  vtkTemplateMacroCase(VTK_SHORT, short, call);                             \
  vtkTemplateMacroCase(VTK_UNSIGNED_SHORT, unsigned short, call);           \
  vtkTemplateMacroCase(VTK_CHAR, char, call);                               \
  vtkTemplateMacroCase(VTK_UNSIGNED_CHAR, unsigned char, call)

#define temDataArrayIteratorMacro(_array, _call)                           \
  temTemplateMacro(                                                        \
    vtkAbstractArray *_aa(_array);                                         \
    if (vtkDataArrayTemplate<VTK_TT> *_dat =                               \
        vtkDataArrayTemplate<VTK_TT>::FastDownCast(_aa))                   \
      {                                                                    \
      typedef VTK_TT vtkDAValueType;                                       \
      typedef vtkDataArrayTemplate<vtkDAValueType> vtkDAContainerType;     \
      typedef vtkDAContainerType::Iterator vtkDAIteratorType;              \
      vtkDAIteratorType vtkDABegin(_dat->Begin());                         \
      vtkDAIteratorType vtkDAEnd(_dat->End());                             \
      _call;                                                               \
      }                                                                    \
    )
}

namespace dax
{
  template<class IteratorType>
  void AccelThreshold(vtkImageData* input, vtkUnstructuredGrid* output,
                      double lower, double upper,
                      IteratorType begin,  IteratorType end,
                      const std::string& dataArrayName)
  {

  typedef typename std::iterator_traits<IteratorType>::value_type ValueType;
  typedef dax::worklet::ThresholdTopology ThresholdTopologyType;
  typedef dax::worklet::ThresholdCount<ValueType> ThresholdCountType;

  dax::cont::UniformGrid<> inputDaxGrid;
  dax::cont::UnstructuredGrid<dax::CellTagHexahedron> outputDaxGrid;

  //convert the vktImage data to a dax uniform grid
  double vtk_origin[3];  input->GetOrigin(vtk_origin);
  double vtk_spacing[3]; input->GetSpacing(vtk_spacing);
  int    vtk_extent[6];  input->GetExtent(vtk_extent);

  inputDaxGrid.SetOrigin(
            dax::Vector3(vtk_origin[0],vtk_origin[1],vtk_origin[2])
            );
  inputDaxGrid.SetSpacing(
            dax::Vector3(vtk_spacing[0],vtk_spacing[1],vtk_spacing[2])
            );
  inputDaxGrid.SetExtent(
            dax::Extent3(dax::make_Id3(vtk_extent[0],vtk_extent[2],vtk_extent[4]),
                         dax::make_Id3(vtk_extent[1],vtk_extent[3],vtk_extent[5]))
            );

  //construct the classify functor
  ThresholdCountType classifyFunctor(static_cast<ValueType>(lower),
                                     static_cast<ValueType>(upper));

  //create an array handle array the begin and end iterators, this creates
  //a view to the data, doesn't copy it
  dax::cont::ArrayHandle<ValueType> thresholdInputValues =
        dax::cont::make_ArrayHandle(begin, std::distance(begin,end) );

  //run the classify step over the entire data set
  dax::cont::ArrayHandle<dax::Id> count;
  dax::cont::DispatcherMapCell< ThresholdCountType > classify(classifyFunctor);
  classify.Invoke(inputDaxGrid, thresholdInputValues, count);

  dax::cont::DispatcherGenerateTopology< ThresholdTopologyType >
        topoDispatcher(count);
  topoDispatcher.Invoke(inputDaxGrid,outputDaxGrid);

  if(outputDaxGrid.GetNumberOfCells() > 0)
    {
    // //get the reduced output threshold point field
    dax::cont::ArrayHandle<ValueType> resultHandle;
    topoDispatcher.CompactPointField(thresholdInputValues,resultHandle);

    //convert the resultHandle to a vtkDataArray
    vtkDataArray* outputData = make_vtkDataArray(ValueType());
    outputData->SetName( dataArrayName.c_str() );
    outputData->SetNumberOfTuples(resultHandle.GetNumberOfValues());
    outputData->SetNumberOfComponents(1);

    //copy the resultHandle into OutputData
    resultHandle.CopyInto(
                  reinterpret_cast<ValueType*>(outputData->GetVoidPointer(0)));

    //convert the outputDaxGrid to a vtkUnstructuredGrid
    convertPoints(outputDaxGrid,output);
    convertCells(outputDaxGrid,output);

    //Assign the vtkDataArray to the vtkUnstructuredGrid.
    vtkPointData *pd = output->GetPointData();
    pd->AddArray(outputData);
    pd->SetActiveScalars( dataArrayName.c_str() );

    outputData->FastDelete();
    }
  }
}

vtkStandardNewMacro(vtkAccelThreshold)

//------------------------------------------------------------------------------
vtkAccelThreshold::vtkAccelThreshold()
{

}

//------------------------------------------------------------------------------
vtkAccelThreshold::~vtkAccelThreshold()
{
}

//------------------------------------------------------------------------------
void vtkAccelThreshold::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//------------------------------------------------------------------------------
int vtkAccelThreshold::RequestData(vtkInformation *request,
                                 vtkInformationVector **inputVector,
                                 vtkInformationVector *outputVector)
{
  vtkImageData *input = vtkImageData::GetData(inputVector[0],0);
  vtkUnstructuredGrid* output = vtkUnstructuredGrid::GetData(outputVector);

  vtkDataArray *inScalars = this->GetInputArrayToProcess(0,inputVector);

  if(!input || !output)
    {
    //fallback to serial threshold if we don't have image data
    return this->Superclass::RequestData(request,inputVector,outputVector);
    }

  std::string scalarsName(inScalars->GetName());

  switch (inScalars->GetDataType())
      {
      temDataArrayIteratorMacro( inScalars,
            dax::AccelThreshold(input,
                                output,
                                this->GetLowerThreshold(),
                                this->GetUpperThreshold(),
                                vtkDABegin,
                                vtkDAEnd,
                                scalarsName) );
      default:
        break;
      }

  return 1;
}