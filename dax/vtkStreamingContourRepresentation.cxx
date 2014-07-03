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
#include "vtkStreamingContourRepresentation.h"

#include "vtkAlgorithmOutput.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataPipeline.h"
#include "vtkGeometryRepresentation.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkPVLODActor.h"
#include "vtkPVRenderView.h"
#include "vtkPVStreamingMacros.h"
#include "vtkPVTrivialProducer.h"
#include "vtkRenderer.h"
#include "vtkStreamingWorker.h"

#include <algorithm>
#include <assert.h>
#include <vector>


vtkStandardNewMacro(vtkStreamingContourRepresentation);
//----------------------------------------------------------------------------
vtkStreamingContourRepresentation::vtkStreamingContourRepresentation()
{
  this->ContourValue = 90;
  this->StreamingCapablePipeline = false;
  this->InStreamingUpdate = false;

  this->Worker = vtkSmartPointer<vtkStreamingWorker>::New();
  this->Mapper = vtkSmartPointer<vtkPolyDataMapper>::New();

  this->Actor = vtkSmartPointer<vtkPVLODActor>::New();
  this->Actor->SetMapper(this->Mapper);
  this->Actor->GetProperty()->SetRepresentationToSurface();
  this->Actor->GetProperty()->SetAmbient(1.0);
  this->Actor->GetProperty()->SetDiffuse(0.0);
  this->Actor->GetProperty()->SetSpecular(0.0);
  this->Actor->SetPickable(0);
}

//----------------------------------------------------------------------------
vtkStreamingContourRepresentation::~vtkStreamingContourRepresentation()
{
}

//----------------------------------------------------------------------------
void vtkStreamingContourRepresentation::SetVisibility(bool val)
{
  this->Actor->SetVisibility(val);
  this->Superclass::SetVisibility(val);
}

//----------------------------------------------------------------------------
void vtkStreamingContourRepresentation::SetOpacity(double val)
{
  this->Actor->GetProperty()->SetOpacity(val);
}

//----------------------------------------------------------------------------
int vtkStreamingContourRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo)
{
  // always forward to superclass first. Superclass returns 0 if the
  // representation is not visible (among other things). In which case there's
  // nothing to do.
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
    {
    return 0;
    }

  if (request_type == vtkPVView::REQUEST_UPDATE())
    {
    // Standard representation stuff, first.
    // 1. Provide the data being rendered.
    vtkPVRenderView::SetPiece(inInfo, this, this->ProcessedData);

    // 2. Provide the bounds.
    double bounds[6];
    this->DataBounds.GetBounds(bounds);

    // Just let the view know of out data bounds.
    vtkPVRenderView::SetGeometryBounds(inInfo, bounds);

    // The only thing extra we need to do here is that we need to let the view
    // know that this representation is streaming capable (or not).
    vtkPVRenderView::SetStreamable(inInfo, this, this->GetStreamingCapablePipeline());
    }
  else if (request_type == vtkPVRenderView::REQUEST_STREAMING_UPDATE())
    {
    if (this->GetStreamingCapablePipeline())
      {
      // This is a streaming update request, request next piece.
      double view_planes[24];
      inInfo->Get(vtkPVRenderView::VIEW_PLANES(), view_planes);
      if (this->StreamingUpdate(view_planes))
        {
        // since we indeed "had" a next piece to produce, give it to the view
        // so it can deliver it to the rendering nodes.
        vtkPVRenderView::SetNextStreamedPiece(
          inInfo, this, this->ProcessedPiece);
        }
      }
    }
  else if (request_type == vtkPVView::REQUEST_RENDER())
    {
    if (this->RenderedData == NULL)
      {
      vtkStreamingStatusMacro(<< this << ": cloning delivered data.");
      vtkAlgorithmOutput* producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this);
      vtkAlgorithm* producer = producerPort->GetProducer();

      this->RenderedData =
        producer->GetOutputDataObject(producerPort->GetIndex());
      this->Mapper->SetInputDataObject(this->RenderedData);
      }
    }

  else if (request_type == vtkPVRenderView::REQUEST_PROCESS_STREAMED_PIECE())
    {
    assert (this->RenderedData != NULL);
    vtkStreamingStatusMacro( << this << ": received new piece.");

    this->RenderedData = this->Worker->GetFinishedPieces();
    this->DataBounds.SetBounds(this->RenderedData->GetBounds());
    this->Mapper->SetInputDataObject(this->RenderedData);
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkStreamingContourRepresentation::RequestInformation(vtkInformation *rqst,
    vtkInformationVector **inputVector,
    vtkInformationVector *outputVector)
{
  // Determine if the input is streaming capable. Unlike most streaming
  // representations we don't care that our input isn't a multi-block. Instead
  // all we care about is that the view has streaming enabled. We can ignore
  // the multi-block requirements since we do our own subdivision.

  this->StreamingCapablePipeline = false;
  if (inputVector[0]->GetNumberOfInformationObjects() == 1)
    {
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
    if (vtkPVView::GetEnableStreaming())
      {
      this->StreamingCapablePipeline = true;
      }
    }

  vtkStreamingStatusMacro(
    << this << ": streaming capable input pipeline? "
    << (this->StreamingCapablePipeline? "yes" : "no"));
  return this->Superclass::RequestInformation(rqst, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkStreamingContourRepresentation::RequestUpdateExtent(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  //we are doing our own block delivery so we have no need to
  //ask the input to give us more blocks
  return this->Superclass::RequestUpdateExtent(request,
                                               inputVector,
                                               outputVector);
}


//----------------------------------------------------------------------------
int vtkStreamingContourRepresentation::RequestData(vtkInformation *rqst,
  vtkInformationVector **inputVector, vtkInformationVector *outputVector)
{
  if (inputVector[0]->GetNumberOfInformationObjects() == 1)
    {
    vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
    if (this->GetStreamingCapablePipeline() && !this->GetInStreamingUpdate())
      {
      // Since the representation re-executed, it means that the input changed
      // and we should initialize our streaming.
      vtkImageData *inputImage = vtkImageData::GetData(inputVector[0],0);
      vtkDataArray *inScalars = this->GetInputArrayToProcess(0,inputVector);

      this->Worker->StartContour(inputImage,inScalars,this->GetContourValue());
      }
    }

  this->ProcessedPiece = 0;
  if (inputVector[0]->GetNumberOfInformationObjects() == 1)
    {
    // Do the streaming independent "transformation" of the data here, in our
    // case, generate the polydata from the input.

    vtkImageData *input = vtkImageData::GetData(inputVector[0],0);
    if (!this->GetInStreamingUpdate())
      {
        this->ProcessedData = vtkSmartPointer<vtkImageData>::New();
        this->ProcessedData->ShallowCopy(input);

        this->DataBounds.SetBounds(input->GetBounds());
      }
    else
      {
      this->ProcessedPiece = input;
      }
    }
  else
    {
    // create an empty dataset. This is needed so that view knows what dataset
    // to expect from the other processes on this node.
    this->ProcessedData = vtkSmartPointer<vtkImageData>::New();
    this->DataBounds.Reset();
    }

  if (!this->GetInStreamingUpdate())
    {
    this->RenderedData = 0;

    // provide the mapper with an empty input. This is needed only because
    // mappers die when input is NULL, currently.
    vtkNew<vtkPolyData> tmp;
    this->Mapper->SetInputDataObject(tmp.GetPointer());
    }

  return this->Superclass::RequestData(rqst, inputVector, outputVector);
}

//----------------------------------------------------------------------------
bool vtkStreamingContourRepresentation::StreamingUpdate(const double view_planes[24])
{
  assert(this->InStreamingUpdate == false);

  if (this->Worker->IsFinished())
    {
    return false;
    }

  // We've determined we need to request something. Do it.
  this->InStreamingUpdate = true;
  vtkStreamingStatusMacro(<< this << ": doing streaming-update.");

  // This ensure that the representation re-executes.
  this->MarkModified();

  // Execute the pipeline.
  this->Update();

  this->InStreamingUpdate = false;
  return true;
}

//----------------------------------------------------------------------------
int vtkStreamingContourRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{

  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");

  // Saying INPUT_IS_OPTIONAL() is essential, since representations don't have
  // any inputs on client-side (in client-server, client-render-server mode) and
  // render-server-side (in client-render-server mode).
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);

  return 1;
}

//----------------------------------------------------------------------------
bool vtkStreamingContourRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->AddActor(this->Actor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkStreamingContourRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
    {
    rview->GetRenderer()->RemoveActor(this->Actor);
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkStreamingContourRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "StreamingCapablePipeline: " << this->StreamingCapablePipeline << endl;
}

//----------------------------------------------------------------------------
void vtkStreamingContourRepresentation::SetInputArrayToProcess(
  int idx, int port, int connection, int fieldAssociation, const char *name)
{
  this->Superclass::SetInputArrayToProcess(
    idx, port, connection, fieldAssociation, name);

  if (name && name[0])
    {
    this->Mapper->SetScalarVisibility(1);
    this->Mapper->SelectColorArray(name);
    this->Mapper->SetUseLookupTableScalarRange(1);
    }
  else
    {
    this->Mapper->SetScalarVisibility(0);
    this->Mapper->SelectColorArray(static_cast<const char*>(NULL));
    }

  switch (fieldAssociation)
    {
  case vtkDataObject::FIELD_ASSOCIATION_CELLS:
    this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
    break;

  case vtkDataObject::FIELD_ASSOCIATION_POINTS:
  default:
    this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
    break;
    }
}

//----------------------------------------------------------------------------
void vtkStreamingContourRepresentation::SetLookupTable(vtkScalarsToColors* lut)
{
  this->Mapper->SetLookupTable(lut);
}

//----------------------------------------------------------------------------
void vtkStreamingContourRepresentation::SetPointSize(double val)
{
  this->Actor->GetProperty()->SetPointSize(val);
}
