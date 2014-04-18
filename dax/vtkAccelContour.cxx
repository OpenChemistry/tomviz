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
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"

vtkStandardNewMacro(vtkAccelContour)

//----------------------------------------------------------------------------
class vtkAccelContour::AccelInternals
{
public:
  AccelInternals()
  {

  }
};

//----------------------------------------------------------------------------
vtkAccelContour::vtkAccelContour():
  Value(0),
  Internals( new vtkAccelContour::AccelInternals() )
{

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
  return 1;
}

//----------------------------------------------------------------------------
int vtkAccelContour::RequestUpdateExtent(vtkInformation*,
                        vtkInformationVector**,
                        vtkInformationVector*)
{
  return 1;
}


//----------------------------------------------------------------------------
int vtkAccelContour::FillInputPortInformation(int, vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  return 1;
}
