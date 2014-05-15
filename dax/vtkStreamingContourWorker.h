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
#ifndef __vtkStreamingContourWorker_h
#define __vtkStreamingContourWorker_h

#include "vtkObject.h"
#include "vtkSmartPointer.h"

#include <vector>

class vtkImageData;
class vtkDataArray;
class vtkPolyData;


//how is this going to interact with the view, do
//we always have to state


class vtkStreamingContourWorker : public vtkObject
{
public:
  vtkTypeMacro(vtkStreamingContourWorker,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkStreamingContourWorker *New();

  //start the volume subdivision and contour algorithm
  void Start(vtkImageData*, vtkDataArray* data, double isoValue);

  //pass all the finished contours back to the caller
  //we don't hold any reference of these polydata internally
  //so once the caller is finished with them they will
  //be deleted.
  std::vector< vtkSmartPointer< vtkPolyData > > GetFinishedPieces();

  //ask if we any sections of the volume left to contour
  bool IsFinished() const;

protected:
  vtkStreamingContourWorker();
  ~vtkStreamingContourWorker();

private:
  class WorkerInternals;
  WorkerInternals* Internals;

  vtkStreamingContourWorker(const vtkStreamingContourWorker&); // Not implemented.
  void operator=(const vtkStreamingContourWorker&);  // Not implemented.

};
#endif
