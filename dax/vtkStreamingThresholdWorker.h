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
#ifndef __vtkStreamingThresholdWorker_h
#define __vtkStreamingThresholdWorker_h

#include "vtkObject.h"
#include "vtkSmartPointer.h"

#include <vector>

class vtkImageData;
class vtkDataArray;
class vtkDataObject;


//how is this going to interact with the view, do
//we always have to state


class vtkStreamingThresholdWorker : public vtkObject
{
public:
  vtkTypeMacro(vtkStreamingThresholdWorker,vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);
  static vtkStreamingThresholdWorker *New();

  //start the volume subdivision and contour algorithm
  void Start(vtkImageData*, vtkDataArray* data, double isoValue);

  //pass as much of the contour back to the streamer for it
  //to render, we will keep modifying the data object to add
  //new data as contours get finished.
  vtkDataObject* GetFinishedPieces();

  //ask if we any sections of the volume left to contour
  bool IsFinished() const;

protected:
  vtkStreamingThresholdWorker();
  ~vtkStreamingThresholdWorker();

private:
  class WorkerInternals;
  WorkerInternals* Internals;

  vtkStreamingThresholdWorker(const vtkStreamingThresholdWorker&); // Not implemented.
  void operator=(const vtkStreamingThresholdWorker&);  // Not implemented.

};
#endif
