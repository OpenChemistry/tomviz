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
#include "vtkAccelContour.h"

#include "vtkDataSet.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"


#include "vtkDataSetAttributes.h"
#include "vtkPointData.h"

#include "SubdividedVolume.h"

#include <iostream>

vtkStandardNewMacro(vtkAccelContour)

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
      (void)vtkDABegin; /* Prevent warnings when unused */                 \
      (void)vtkDAEnd;                                                      \
      _call;                                                               \
      }                                                                    \
    )
}

//----------------------------------------------------------------------------
class vtkAccelContour::AccelInternals
{
public:
  AccelInternals(std::size_t numSubGridsPerDim):
    Volume(),
    NumSubGridsPerDim(numSubGridsPerDim)
  {
  }

  bool IsValid() const { return this->Volume.numSubGrids() > 0; }

  template <class IteratorType, class LoggerType>
  bool CreateSearchStructure( vtkImageData *input,
                              const IteratorType begin,
                              const IteratorType end,
                              LoggerType& logger)
  {
    logger << "CreateSearchStructure" << std::endl;
    this->Volume = TEM::accel::SubdividedVolume( NumSubGridsPerDim,
                                                 input,
                                                 logger );
    logger << "ComputeHighLows" << std::endl;
    this->Volume.ComputeHighLows( begin, end, logger );
    return true;
  }

  template <class IteratorType,  class LoggerType>
  bool Contour(double v,
               const IteratorType begin,
               const IteratorType end,
               LoggerType& logger)
  {
    logger << "Contour" << std::endl;
    return true;
  }
private:
  TEM::accel::SubdividedVolume Volume;
  std::size_t NumSubGridsPerDim;
};


//----------------------------------------------------------------------------
vtkAccelContour::vtkAccelContour():
  Value(0),
  Internals( new vtkAccelContour::AccelInternals(4) )
{

  this->SetInputArrayToProcess(0,0,0,vtkDataObject::FIELD_ASSOCIATION_POINTS_THEN_CELLS,
                               vtkDataSetAttributes::SCALARS);

}

//----------------------------------------------------------------------------
vtkAccelContour::~vtkAccelContour()
{
  delete this->Internals;
}

//------------------------------------------------------------------------------
void vtkAccelContour::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}


//----------------------------------------------------------------------------
int vtkAccelContour::RequestData(vtkInformation* request,
                vtkInformationVector** inputVector,
                vtkInformationVector* outputVector)
{
   std::fstream msglog("/Users/robert/contour.log",
                       std::fstream::in | std::fstream::out);
   msglog << "vtkAccelContour::RequestData" << std::endl;


  vtkDataArray *inScalars = NULL;

  //determine if we need to construct the volume
  if( !this->Internals->IsValid() )
    {
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
    vtkImageData *input = vtkImageData::SafeDownCast(
                            inInfo->Get(vtkDataObject::DATA_OBJECT()));
    if(!input)
      {
      msglog << "invalid input data " << std::endl;
      msglog.close();
      return 1;
      }

    inScalars = this->GetInputArrayToProcess(0,inputVector);
    if(!inScalars)
      {
      msglog << "inScalars = input->GetPointData()->GetScalars()" << std::endl;
      inScalars = input->GetPointData()->GetScalars();
      }

    msglog << "valid inScalars " << std::endl;
    switch (inScalars->GetDataType())
      {
      temDataArrayIteratorMacro( inScalars, this->Internals->CreateSearchStructure(input, vtkDABegin, vtkDAEnd, msglog) );
      default:
        break;
      }
    }

 if(inScalars)
  {
  // call template function
   switch (inScalars->GetDataType())
    {
    temDataArrayIteratorMacro(inScalars, this->Internals->Contour(this->GetValue(), vtkDABegin, vtkDAEnd, msglog));
    default:
      break;
    }
  }

  return 1;
}


//----------------------------------------------------------------------------
int vtkAccelContour::FillInputPortInformation(int, vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObject");
  return 1;
}
