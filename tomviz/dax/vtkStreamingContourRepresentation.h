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

#ifndef __vtkStreamingContourRepresentation_h
#define __vtkStreamingContourRepresentation_h

#include "vtkPVDataRepresentation.h"
#include "vtkSmartPointer.h" // for smart pointer.
#include "vtkWeakPointer.h" // for weak pointer.
#include "vtkBoundingBox.h" // needed for vtkBoundingBox.

class vtkPolyDataMapper;
class vtkMultiBlockDataSet;
class vtkPVLODActor;
class vtkScalarsToColors;
class vtkStreamingWorker;

class vtkStreamingContourRepresentation : public vtkPVDataRepresentation
{
public:
  static vtkStreamingContourRepresentation* New();
  vtkTypeMacro(vtkStreamingContourRepresentation, vtkPVDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set the input data arrays that this algorithm will process. Overridden to
  // pass the array selection to the mapper.
  virtual void SetInputArrayToProcess(int idx, int port, int connection,
    int fieldAssociation, const char *name);
  virtual void SetInputArrayToProcess(int idx, int port, int connection,
    int fieldAssociation, int fieldAttributeType)
    {
    this->Superclass::SetInputArrayToProcess(
      idx, port, connection, fieldAssociation, fieldAttributeType);
    }
  virtual void SetInputArrayToProcess(int idx, vtkInformation *info)
    {
    this->Superclass::SetInputArrayToProcess(idx, info);
    }
  virtual void SetInputArrayToProcess(int idx, int port, int connection,
                              const char* fieldAssociation,
                              const char* attributeTypeorName)
    {
    this->Superclass::SetInputArrayToProcess(idx, port, connection,
      fieldAssociation, attributeTypeorName);
    }

  // Description:
  // Overridden to handle various view passes.
  virtual int ProcessViewRequest(vtkInformationRequestKey* request_type,
    vtkInformation* inInfo, vtkInformation* outInfo);

  // Description:
  // Get/Set the visibility for this representation. When the visibility of
  // representation of false, all view passes are ignored.
  virtual void SetVisibility(bool val);

  // Description:
  // Set the opacity.
  void SetOpacity(double val);

  //---------------------------------------------------------------------------
  // The following API is to simply provide the functionality similar to
  // vtkGeometryRepresentation.
  //---------------------------------------------------------------------------
  void SetLookupTable(vtkScalarsToColors*);
  void SetPointSize(double val);

  // Description:
  // Get and Set the ContourValue to use
  vtkGetMacro(ContourValue,double);
  vtkSetMacro(ContourValue,double);

//BTX
protected:
  vtkStreamingContourRepresentation();
  ~vtkStreamingContourRepresentation();

  // Description:
  // Adds the representation to the view.  This is called from
  // vtkView::AddRepresentation().  Subclasses should override this method.
  // Returns true if the addition succeeds.
  virtual bool AddToView(vtkView* view);

  // Description:
  // Removes the representation to the view.  This is called from
  // vtkView::RemoveRepresentation().  Subclasses should override this method.
  // Returns true if the removal succeeds.
  virtual bool RemoveFromView(vtkView* view);

  // Description:
  // Fill input port information.
  int FillInputPortInformation(int port, vtkInformation* info);

  // Description:
  // Overridden to check if the input pipeline is streaming capable. This method
  // should check if streaming is enabled i.e. vtkPVView::GetEnableStreaming()
  // and the input pipeline provides necessary AMR meta-data.
  virtual int RequestInformation(vtkInformation *rqst,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);

  // Description:
  // Setup the block request. During StreamingUpdate, this will request the
  // blocks based on priorities determined by the vtkStreamingContourPriorityQueue,
  // otherwise it doesn't make any specific request. AMR sources can treat the
  // absence of specific block request to mean various things. It's expected
  // that read only the root block (or a few more) in that case.
  virtual int RequestUpdateExtent(vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector);

  // Description:
  // Generate the outline for the current input.
  // When not in StreamingUpdate, this also initializes the priority queue since
  // the input AMR may have totally changed, including its structure.
  virtual int RequestData(vtkInformation *rqst,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector);

  // Description:
  // Returns true when the input pipeline supports streaming. It is set in
  // RequestInformation().
  vtkGetMacro(StreamingCapablePipeline, bool);

  // Description:
  // Returns true when StreamingUpdate() is being processed.
  vtkGetMacro(InStreamingUpdate, bool);

  // Description:
  // Returns true if this representation has a "next piece" that it streamed.
  // This method will update the PriorityQueue using the view planes specified
  // and then call Update() on the representation, making it reexecute and
  // regenerate the outline for the next "piece" of data.
  bool StreamingUpdate(const double view_planes[24]);

  // Description:
  // This is the data object generated processed by the most recent call to
  // RequestData() while not streaming. In our case this will be a shallow
  // copy of the input data.
  // This is non-empty only on the data-server nodes.
  vtkSmartPointer<vtkDataObject> ProcessedData;

  // Description:
  // This is the data object generated processed by the most recent call to
  // RequestData() while streaming.
  // This is non-empty only on the data-server nodes.
  vtkSmartPointer<vtkDataObject> ProcessedPiece;

  // Description:
  // Helps us keep track of the data being rendered.
  vtkWeakPointer<vtkPolyData> RenderedData;

  // Description:
  // vtkStreamingWorker is a helper class we used to compute the
  // sub-blocks
  vtkSmartPointer<vtkStreamingWorker> Worker;

  // Description:
  // Actor used to render the outlines in the view.
  vtkSmartPointer<vtkPolyDataMapper> Mapper;
  vtkSmartPointer<vtkPVLODActor> Actor;

  // Description:
  // Used to keep track of data bounds.
  vtkBoundingBox DataBounds;

private:
  vtkStreamingContourRepresentation(const vtkStreamingContourRepresentation&); // Not implemented
  void operator=(const vtkStreamingContourRepresentation&); // Not implemented

  // Description:
  // The contour value to operate on
  double ContourValue;

  // Description:
  // This flag is set to true if the input pipeline is streaming capable in
  // RequestInformation(). Note that in client-server mode, this is valid only
  // on the data-server nodes since all other nodes don't have input pipelines
  // connected, they cannot indicate if the pipeline supports streaming.
  bool StreamingCapablePipeline;

  // Description:
  // This flag is used to indicate that the representation is being updated
  // during the streaming pass. RequestData() can use this flag to reset
  // internal datastructures when the input changes for non-streaming reasons
  // and we need to clear our streaming buffers since the streamed data is no
  // longer valid.
  bool InStreamingUpdate;
//ETX
};

#endif
