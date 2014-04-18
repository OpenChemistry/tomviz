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
#ifndef __vtkAccelContour_h
#define __vtkAccelContour_h

#include "vtkPolyDataAlgorithm.h"
#include "vtkNew.h"

class vtkAccelContour : public vtkPolyDataAlgorithm
{
public:
  vtkTypeMacro(vtkAccelContour,vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Construct object with initial range (0,1) and single contour value
  // of 0.0.
  static vtkAccelContour *New();

  //Set and Get the value to contour on
  vtkSetMacro(Value,double);
  vtkGetMacro(Value,double);

protected:
  vtkAccelContour();
  ~vtkAccelContour();

  int RequestData(vtkInformation* request,
                  vtkInformationVector** inputVector,
                  vtkInformationVector* outputVector);

  int RequestUpdateExtent(vtkInformation*,
                          vtkInformationVector**,
                          vtkInformationVector*);

  int FillInputPortInformation(int port, vtkInformation *info);
private:
  double Value;
  class AccelInternals;

  AccelInternals* Internals;

  vtkAccelContour(const vtkAccelContour&); // Not implemented.
  void operator=(const vtkAccelContour&);  // Not implemented.
};

#endif