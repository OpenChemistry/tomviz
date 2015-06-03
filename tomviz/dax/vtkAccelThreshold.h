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

#ifndef tomvizvtkAccelThreshold_h
#define tomvizvtkAccelThreshold_h

#include "vtkThreshold.h"

class  vtkAccelThreshold : public vtkThreshold
{
public:
  vtkTypeMacro(vtkAccelThreshold,vtkThreshold)
  void PrintSelf(ostream& os, vtkIndent indent);

  static vtkAccelThreshold* New();

protected:
  vtkAccelThreshold();
  ~vtkAccelThreshold();

  virtual int RequestData(vtkInformation *,
                          vtkInformationVector **,
                          vtkInformationVector *);

private:
  vtkAccelThreshold(const vtkAccelThreshold&); // Not implemented
  void operator=(const vtkAccelThreshold&); // Not implemented
};

#endif //vtkAccelThreshold