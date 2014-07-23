/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkColorImagePlaneWidget.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkColorImagePlaneWidget.h"

#include "vtkActor.h"
#include "vtkAlgorithmOutput.h"
#include "vtkAssemblyNode.h"
#include "vtkAssemblyPath.h"
#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkCellPicker.h"
#include "vtkImageData.h"
#include "vtkImageReslice.h"
#include "vtkInformation.h"
#include "vtkLookupTable.h"
#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPickingManager.h"
#include "vtkPlane.h"
#include "vtkPlaneCollection.h"
#include "vtkPlaneSource.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkScalarsToColors.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkTexture.h"
#include "vtkTransform.h"

vtkStandardNewMacro(vtkColorImagePlaneWidget);

vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, PlaneProperty, vtkProperty);
vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, SelectedPlaneProperty, vtkProperty);
vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, MarginProperty, vtkProperty);
vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, TexturePlaneProperty, vtkProperty);

//----------------------------------------------------------------------------
vtkColorImagePlaneWidget::vtkColorImagePlaneWidget() : vtkPolyDataSourceWidget()
{
  this->State = vtkColorImagePlaneWidget::Start;
  this->EventCallbackCommand->SetCallback(vtkColorImagePlaneWidget::ProcessEvents);

  this->Interaction              = 1;
  this->PlaneOrientation         = 0;
  this->PlaceFactor              = 1.0;
  this->TextureInterpolate       = 1;
  this->ResliceInterpolate       = VTK_LINEAR_RESLICE;
  this->MarginSizeX              = 0.05;
  this->MarginSizeY              = 0.05;

  // Represent the plane's outline
  //
  this->PlaneSource = vtkPlaneSource::New();
  this->PlaneSource->SetXResolution(1);
  this->PlaneSource->SetYResolution(1);
  this->PlaneOutlinePolyData = vtkPolyData::New();
  this->PlaneOutlineActor    = vtkActor::New();

  // Represent the resliced image plane
  //
  this->Reslice            = vtkImageReslice::New();
  this->Reslice->TransformInputSamplingOff();
  this->Reslice->AutoCropOutputOff();
  this->Reslice->MirrorOff();

  this->ResliceAxes        = vtkMatrix4x4::New();
  this->Texture            = vtkTexture::New();
  this->TexturePlaneActor  = vtkActor::New();
  this->Transform          = vtkTransform::New();
  this->ImageData          = 0;
  this->LookupTable        = 0;

  // Represent the oblique positioning margins
  //
  this->MarginPolyData = vtkPolyData::New();
  this->MarginActor    = vtkActor::New();


  this->GeneratePlaneOutline();

  // Define some default point coordinates
  //
  double bounds[6];
  bounds[0] = -0.5;
  bounds[1] =  0.5;
  bounds[2] = -0.5;
  bounds[3] =  0.5;
  bounds[4] = -0.5;
  bounds[5] =  0.5;

  // Initial creation of the widget, serves to initialize it
  //
  this->PlaceWidget(bounds);

  this->GenerateTexturePlane();
  this->GenerateMargins();

  // Manage the picking stuff
  //
  this->PlanePicker = NULL;
  vtkNew<vtkCellPicker> picker;
  picker->SetTolerance(0.005); //need some fluff
  this->SetPicker(picker.GetPointer());

  // Set up the initial properties
  //
  this->PlaneProperty         = 0;
  this->SelectedPlaneProperty = 0;
  this->TexturePlaneProperty  = 0;
  this->MarginProperty        = 0;
  this->CreateDefaultProperties();

  // Set up actions
  this->LeftButtonAction = vtkColorImagePlaneWidget::VTK_SLICE_MOTION_ACTION;
  this->MiddleButtonAction = vtkColorImagePlaneWidget::VTK_SLICE_MOTION_ACTION;
  this->RightButtonAction = vtkColorImagePlaneWidget::VTK_NO_ACTION;

  this->LastButtonPressed = vtkColorImagePlaneWidget::VTK_NO_BUTTON;

  this->TextureVisibility = 1;
}

//----------------------------------------------------------------------------
vtkColorImagePlaneWidget::~vtkColorImagePlaneWidget()
{
  this->PlaneOutlineActor->Delete();
  this->PlaneOutlinePolyData->Delete();
  this->PlaneSource->Delete();

  if ( this->PlanePicker )
    {
    this->PlanePicker->UnRegister(this);
    }

  if ( this->PlaneProperty )
    {
    this->PlaneProperty->Delete();
    }

  if ( this->SelectedPlaneProperty )
    {
    this->SelectedPlaneProperty->Delete();
    }

  if ( this->MarginProperty )
    {
    this->MarginProperty->Delete();
    }

  this->ResliceAxes->Delete();
  this->Transform->Delete();
  this->Reslice->Delete();

  if ( this->LookupTable )
    {
    this->LookupTable->UnRegister(this);
    }

  this->TexturePlaneActor->Delete();
  this->Texture->Delete();

  if ( this->TexturePlaneProperty )
    {
    this->TexturePlaneProperty->Delete();
    }

  if ( this->ImageData )
    {
    this->ImageData = 0;
    }

  this->MarginActor->Delete();
  this->MarginPolyData->Delete();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetTextureVisibility(int vis)
{
  if (this->TextureVisibility == vis)
    {
    return;
    }

  this->TextureVisibility = vis;

  if ( this->Enabled )
    {
    if (this->TextureVisibility)
      {
      this->CurrentRenderer->AddViewProp(this->TexturePlaneActor);
      }
    else
      {
      this->CurrentRenderer->RemoveViewProp(this->TexturePlaneActor);
      }
    }

  this->Modified();
}


//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetEnabled(int enabling)
{

  if ( ! this->Interactor )
    {
    vtkErrorMacro(<<"The interactor must be set prior to enabling/disabling widget");
    return;
    }

  if ( enabling ) //----------------------------------------------------------
    {
    vtkDebugMacro(<<"Enabling plane widget");

    if ( this->Enabled ) //already enabled, just return
      {
      return;
      }

    if ( ! this->CurrentRenderer )
      {
      this->SetCurrentRenderer(this->Interactor->FindPokedRenderer(
        this->Interactor->GetLastEventPosition()[0],
        this->Interactor->GetLastEventPosition()[1]));
      if (this->CurrentRenderer == NULL)
        {
        return;
        }
      }

    this->Enabled = 1;

    // we have to honour this ivar: it could be that this->Interaction was
    // set to off when we were disabled
    if (this->Interaction)
    {
      this->AddObservers();
    }

    // Add the plane
    this->CurrentRenderer->AddViewProp(this->PlaneOutlineActor);
    this->PlaneOutlineActor->SetProperty(this->PlaneProperty);

    //add the TexturePlaneActor
    if (this->TextureVisibility)
      {
      this->CurrentRenderer->AddViewProp(this->TexturePlaneActor);
      }
    this->TexturePlaneActor->SetProperty(this->TexturePlaneProperty);


    // Add the margins
    this->CurrentRenderer->AddViewProp(this->MarginActor);
    this->MarginActor->SetProperty(this->MarginProperty);

    this->TexturePlaneActor->PickableOn();

    this->InvokeEvent(vtkCommand::EnableEvent,0);

    }

  else //disabling----------------------------------------------------------
    {
    vtkDebugMacro(<<"Disabling plane widget");

    if ( ! this->Enabled ) //already disabled, just return
      {
      return;
      }

    this->Enabled = 0;

    // don't listen for events any more
    this->Interactor->RemoveObserver(this->EventCallbackCommand);

    // turn off the plane
    this->CurrentRenderer->RemoveViewProp(this->PlaneOutlineActor);

    //turn off the texture plane
    this->CurrentRenderer->RemoveViewProp(this->TexturePlaneActor);

    //turn off the margins
    this->CurrentRenderer->RemoveViewProp(this->MarginActor);

    this->TexturePlaneActor->PickableOff();

    this->InvokeEvent(vtkCommand::DisableEvent,0);
    this->SetCurrentRenderer(NULL);
    }

  this->Interactor->Render();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::ProcessEvents(vtkObject* vtkNotUsed(object),
                                        unsigned long event,
                                        void* clientdata,
                                        void* vtkNotUsed(calldata))
{
  vtkColorImagePlaneWidget* self =
    reinterpret_cast<vtkColorImagePlaneWidget *>( clientdata );

  self->LastButtonPressed = vtkColorImagePlaneWidget::VTK_NO_BUTTON;

  //okay, let's do the right thing
  switch ( event )
    {
    case vtkCommand::LeftButtonPressEvent:
      self->LastButtonPressed = vtkColorImagePlaneWidget::VTK_LEFT_BUTTON;
      self->OnLeftButtonDown();
      break;
    case vtkCommand::LeftButtonReleaseEvent:
      self->LastButtonPressed = vtkColorImagePlaneWidget::VTK_LEFT_BUTTON;
      self->OnLeftButtonUp();
      break;
    case vtkCommand::MiddleButtonPressEvent:
      self->LastButtonPressed = vtkColorImagePlaneWidget::VTK_MIDDLE_BUTTON;
      self->OnMiddleButtonDown();
      break;
    case vtkCommand::MiddleButtonReleaseEvent:
      self->LastButtonPressed = vtkColorImagePlaneWidget::VTK_MIDDLE_BUTTON;
      self->OnMiddleButtonUp();
      break;
    case vtkCommand::RightButtonPressEvent:
      self->LastButtonPressed = vtkColorImagePlaneWidget::VTK_RIGHT_BUTTON;
      self->OnRightButtonDown();
      break;
    case vtkCommand::RightButtonReleaseEvent:
      self->LastButtonPressed = vtkColorImagePlaneWidget::VTK_RIGHT_BUTTON;
      self->OnRightButtonUp();
      break;
    case vtkCommand::MouseMoveEvent:
      self->OnMouseMove();
      break;
    case vtkCommand::CharEvent:
      break;
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::AddObservers(void)
{
  // listen for the following events
  vtkRenderWindowInteractor *i = this->Interactor;
  if (i)
    {
    i->AddObserver(vtkCommand::MouseMoveEvent, this->EventCallbackCommand,
                       this->Priority);
    i->AddObserver(vtkCommand::LeftButtonPressEvent,
                       this->EventCallbackCommand, this->Priority);
    i->AddObserver(vtkCommand::LeftButtonReleaseEvent,
                       this->EventCallbackCommand, this->Priority);
    i->AddObserver(vtkCommand::MiddleButtonPressEvent,
                       this->EventCallbackCommand, this->Priority);
    i->AddObserver(vtkCommand::MiddleButtonReleaseEvent,
                       this->EventCallbackCommand, this->Priority);
    i->AddObserver(vtkCommand::RightButtonPressEvent,
                       this->EventCallbackCommand, this->Priority);
    i->AddObserver(vtkCommand::RightButtonReleaseEvent,
                       this->EventCallbackCommand, this->Priority);
    i->AddObserver(vtkCommand::CharEvent,
                       this->EventCallbackCommand, this->Priority);
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetInteraction(int interact)
{
  if (this->Interactor && this->Enabled)
    {
    if (this->Interaction == interact)
      {
      return;
      }
    if (interact == 0)
      {
      this->Interactor->RemoveObserver(this->EventCallbackCommand);
      }
    else
      {
        this->AddObservers();
      }
    this->Interaction = interact;
    }
  else
    {
    vtkGenericWarningMacro(<<"set interactor and Enabled before changing interaction...");
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if ( this->PlaneProperty )
    {
    os << indent << "Plane Property:\n";
    this->PlaneProperty->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "Plane Property: (none)\n";
    }

  if ( this->SelectedPlaneProperty )
    {
    os << indent << "Selected Plane Property:\n";
    this->SelectedPlaneProperty->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "Selected Plane Property: (none)\n";
    }

  if ( this->LookupTable )
    {
    os << indent << "LookupTable:\n";
    this->LookupTable->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "LookupTable: (none)\n";
    }

  if ( this->MarginProperty )
    {
    os << indent << "Margin Property:\n";
    this->MarginProperty->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "Margin Property: (none)\n";
    }

  if ( this->TexturePlaneProperty )
    {
    os << indent << "TexturePlane Property:\n";
    this->TexturePlaneProperty->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "TexturePlane Property: (none)\n";
    }

  if ( this->Reslice )
    {
    os << indent << "Reslice:\n";
    this->Reslice->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "Reslice: (none)\n";
    }

  if ( this->ResliceAxes )
    {
    os << indent << "ResliceAxes:\n";
    this->ResliceAxes->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "ResliceAxes: (none)\n";
    }

  double *o = this->PlaneSource->GetOrigin();
  double *pt1 = this->PlaneSource->GetPoint1();
  double *pt2 = this->PlaneSource->GetPoint2();

  os << indent << "Origin: (" << o[0] << ", "
     << o[1] << ", "
     << o[2] << ")\n";
  os << indent << "Point 1: (" << pt1[0] << ", "
     << pt1[1] << ", "
     << pt1[2] << ")\n";
  os << indent << "Point 2: (" << pt2[0] << ", "
     << pt2[1] << ", "
     << pt2[2] << ")\n";

  os << indent << "Plane Orientation: " << this->PlaneOrientation << "\n";
  os << indent << "Reslice Interpolate: " << this->ResliceInterpolate << "\n";
  os << indent << "Texture Interpolate: "
     << (this->TextureInterpolate ? "On\n" : "Off\n") ;
  os << indent << "Texture Visibility: "
     << (this->TextureVisibility ? "On\n" : "Off\n") ;
  os << indent << "Interaction: "
     << (this->Interaction ? "On\n" : "Off\n") ;
  os << indent << "LeftButtonAction: " << this->LeftButtonAction << endl;
  os << indent << "MiddleButtonAction: " << this->MiddleButtonAction << endl;
  os << indent << "RightButtonAction: " << this->RightButtonAction << endl;

  os << indent << "MarginSizeX: "
     << this->MarginSizeX << "\n";
  os << indent << "MarginSizeY: "
     << this->MarginSizeY << "\n";
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::BuildRepresentation()
{
  this->PlaneSource->Update();
  double *o = this->PlaneSource->GetOrigin();
  double *pt1 = this->PlaneSource->GetPoint1();
  double *pt2 = this->PlaneSource->GetPoint2();

  double x[3];
  x[0] = o[0] + (pt1[0]-o[0]) + (pt2[0]-o[0]);
  x[1] = o[1] + (pt1[1]-o[1]) + (pt2[1]-o[1]);
  x[2] = o[2] + (pt1[2]-o[2]) + (pt2[2]-o[2]);

  vtkPoints* points = this->PlaneOutlinePolyData->GetPoints();
  points->SetPoint(0,o);
  points->SetPoint(1,pt1);
  points->SetPoint(2,x);
  points->SetPoint(3,pt2);
  points->GetData()->Modified();
  this->PlaneOutlinePolyData->Modified();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::HighlightPlane(int highlight)
{
  if ( highlight )
    {
    this->PlaneOutlineActor->SetProperty(this->SelectedPlaneProperty);
    this->PlanePicker->GetPickPosition(this->LastPickPosition);
    }
  else
    {
    this->PlaneOutlineActor->SetProperty(this->PlaneProperty);
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::OnButtonDown(int *btn)
{
  switch (*btn)
    {
    case vtkColorImagePlaneWidget::VTK_NO_ACTION:
      break;
    case vtkColorImagePlaneWidget::VTK_SLICE_MOTION_ACTION:
      this->StartSliceMotion();
      break;
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::OnButtonUp(int *btn)
{
  switch (*btn)
    {
    case vtkColorImagePlaneWidget::VTK_NO_ACTION:
      break;
    case vtkColorImagePlaneWidget::VTK_SLICE_MOTION_ACTION:
      this->StopSliceMotion();
      break;
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::StartSliceMotion()
{
  int X = this->Interactor->GetEventPosition()[0];
  int Y = this->Interactor->GetEventPosition()[1];

  // Okay, make sure that the pick is in the current renderer
  if (!this->CurrentRenderer || !this->CurrentRenderer->IsInViewport(X, Y))
    {
    this->State = vtkColorImagePlaneWidget::Outside;
    return;
    }

  // Okay, we can process this. If anything is picked, then we
  // can start pushing or check for adjusted states.
  vtkAssemblyPath* path = this->GetAssemblyPath(X, Y, 0., this->PlanePicker);

  int found = 0;
  int i;
  if ( path != 0 )
    {
    // Deal with the possibility that we may be using a shared picker
    vtkCollectionSimpleIterator sit;
    path->InitTraversal(sit);
    vtkAssemblyNode *node;
    for(i = 0; i< path->GetNumberOfItems() && !found ;i++)
      {
      node = path->GetNextNode(sit);
      if(node->GetViewProp() == vtkProp::SafeDownCast(this->TexturePlaneActor) )
        {
        found = 1;
        }
      }
    }

  if ( !found || path == 0 )
    {
    this->State = vtkColorImagePlaneWidget::Outside;
    this->HighlightPlane(0);
    this->ActivateMargins(0);
    return;
    }
  else
    {
    this->State = vtkColorImagePlaneWidget::Pushing;
    this->HighlightPlane(1);
    this->ActivateMargins(1);
    this->AdjustState();
    this->UpdateMargins();
    }

  this->EventCallbackCommand->SetAbortFlag(1);
  this->StartInteraction();
  this->InvokeEvent(vtkCommand::StartInteractionEvent,0);
  this->Interactor->Render();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::StopSliceMotion()
{
  if ( this->State == vtkColorImagePlaneWidget::Outside ||
       this->State == vtkColorImagePlaneWidget::Start )
    {
    return;
    }

  this->State = vtkColorImagePlaneWidget::Start;
  this->HighlightPlane(0);
  this->ActivateMargins(0);

  this->EventCallbackCommand->SetAbortFlag(1);
  this->EndInteraction();
  this->InvokeEvent(vtkCommand::EndInteractionEvent,0);
  this->Interactor->Render();
}


//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::OnMouseMove()
{
  // See whether we're active
  //
  if ( this->State == vtkColorImagePlaneWidget::Outside ||
       this->State == vtkColorImagePlaneWidget::Start )
    {
    return;
    }

  int X = this->Interactor->GetEventPosition()[0];
  int Y = this->Interactor->GetEventPosition()[1];

  // Do different things depending on state
  // Calculations everybody does
  //
  double focalPoint[4], pickPoint[4], prevPickPoint[4];
  double z, vpn[3];

  vtkCamera *camera = this->CurrentRenderer->GetActiveCamera();
  if ( ! camera )
    {
    return;
    }

  // Compute the two points defining the motion vector
  //
  this->ComputeWorldToDisplay(this->LastPickPosition[0],
                              this->LastPickPosition[1],
                              this->LastPickPosition[2], focalPoint);
  z = focalPoint[2];

  this->ComputeDisplayToWorld(
    double(this->Interactor->GetLastEventPosition()[0]),
    double(this->Interactor->GetLastEventPosition()[1]),
    z, prevPickPoint);

  this->ComputeDisplayToWorld(double(X), double(Y), z, pickPoint);

  if ( this->State == vtkColorImagePlaneWidget::Pushing )
    {
    this->Push(prevPickPoint, pickPoint);
    this->UpdatePlane();
    this->UpdateMargins();
    this->BuildRepresentation();
    }
  else if ( this->State == vtkColorImagePlaneWidget::Rotating )
    {
    camera->GetViewPlaneNormal(vpn);
    this->Rotate(double(X), double(Y), prevPickPoint, pickPoint, vpn);
    this->UpdatePlane();
    this->UpdateMargins();
    this->BuildRepresentation();
    }
  else if ( this->State == vtkColorImagePlaneWidget::Scaling )
    {
    this->Scale(prevPickPoint, pickPoint, X, Y);
    this->UpdatePlane();
    this->UpdateMargins();
    this->BuildRepresentation();
    }

  // Interact, if desired
  //
  this->EventCallbackCommand->SetAbortFlag(1);
  this->InvokeEvent(vtkCommand::InteractionEvent,0);

  this->Interactor->Render();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::Push(double *p1, double *p2)
{
  // Get the motion vector
  //
  double v[3];
  v[0] = p2[0] - p1[0];
  v[1] = p2[1] - p1[1];
  v[2] = p2[2] - p1[2];

  this->PlaneSource->Push( vtkMath::Dot( v, this->PlaneSource->GetNormal() ) );
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::CreateDefaultProperties()
{
  if ( ! this->PlaneProperty )
    {
    this->PlaneProperty = vtkProperty::New();
    this->PlaneProperty->SetAmbient(1);
    this->PlaneProperty->SetColor(1,1,1);
    this->PlaneProperty->SetRepresentationToWireframe();
    this->PlaneProperty->SetInterpolationToFlat();
    }

  if ( ! this->SelectedPlaneProperty )
    {
    this->SelectedPlaneProperty = vtkProperty::New();
    this->SelectedPlaneProperty->SetAmbient(1);
    this->SelectedPlaneProperty->SetColor(0,1,0);
    this->SelectedPlaneProperty->SetRepresentationToWireframe();
    this->SelectedPlaneProperty->SetInterpolationToFlat();
    }

  if ( ! this->MarginProperty )
    {
    this->MarginProperty = vtkProperty::New();
    this->MarginProperty->SetAmbient(1);
    this->MarginProperty->SetColor(0,0,1);
    this->MarginProperty->SetRepresentationToWireframe();
    this->MarginProperty->SetInterpolationToFlat();
    }

  if ( ! this->TexturePlaneProperty )
    {
    this->TexturePlaneProperty = vtkProperty::New();
    this->TexturePlaneProperty->SetAmbient(1);
    this->TexturePlaneProperty->SetInterpolationToFlat();
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::PlaceWidget(double bds[6])
{
  double bounds[6], center[3];

  this->AdjustBounds(bds, bounds, center);

  if ( this->PlaneOrientation == 1 )
    {
    this->PlaneSource->SetOrigin(bounds[0],center[1],bounds[4]);
    this->PlaneSource->SetPoint1(bounds[1],center[1],bounds[4]);
    this->PlaneSource->SetPoint2(bounds[0],center[1],bounds[5]);
    }
  else if ( this->PlaneOrientation == 2 )
    {
    this->PlaneSource->SetOrigin(bounds[0],bounds[2],center[2]);
    this->PlaneSource->SetPoint1(bounds[1],bounds[2],center[2]);
    this->PlaneSource->SetPoint2(bounds[0],bounds[3],center[2]);
    }
  else //default or x-normal
    {
    this->PlaneSource->SetOrigin(center[0],bounds[2],bounds[4]);
    this->PlaneSource->SetPoint1(center[0],bounds[3],bounds[4]);
    this->PlaneSource->SetPoint2(center[0],bounds[2],bounds[5]);
    }

  this->UpdatePlane();
  this->BuildRepresentation();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetPlaneOrientation(int i)
{
  // Generate a XY plane if i = 2, z-normal
  // or a YZ plane if i = 0, x-normal
  // or a ZX plane if i = 1, y-normal
  //
  this->PlaneOrientation = i;

  // This method must be called _after_ SetInput
  //
  if ( !this->ImageData )
    {
    vtkErrorMacro(<<"SetInput() before setting plane orientation.");
    return;
    }

  vtkAlgorithm* inpAlg = this->Reslice->GetInputAlgorithm();
  inpAlg->UpdateInformation();
  vtkInformation* outInfo = inpAlg->GetOutputInformation(0);
  int extent[6];
  outInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), extent);
  double origin[3];
  outInfo->Get(vtkDataObject::ORIGIN(), origin);
  double spacing[3];
  outInfo->Get(vtkDataObject::SPACING(), spacing);

  // Prevent obscuring voxels by offsetting the plane geometry
  //
  double xbounds[] = {origin[0] + spacing[0] * (extent[0] - 0.5),
                     origin[0] + spacing[0] * (extent[1] + 0.5)};
  double ybounds[] = {origin[1] + spacing[1] * (extent[2] - 0.5),
                     origin[1] + spacing[1] * (extent[3] + 0.5)};
  double zbounds[] = {origin[2] + spacing[2] * (extent[4] - 0.5),
                     origin[2] + spacing[2] * (extent[5] + 0.5)};

  if ( spacing[0] < 0.0 )
    {
    double t = xbounds[0];
    xbounds[0] = xbounds[1];
    xbounds[1] = t;
    }
  if ( spacing[1] < 0.0 )
    {
    double t = ybounds[0];
    ybounds[0] = ybounds[1];
    ybounds[1] = t;
    }
  if ( spacing[2] < 0.0 )
    {
    double t = zbounds[0];
    zbounds[0] = zbounds[1];
    zbounds[1] = t;
    }

  if ( i == 2 ) //XY, z-normal
    {
    this->PlaneSource->SetOrigin(xbounds[0],ybounds[0],zbounds[0]);
    this->PlaneSource->SetPoint1(xbounds[1],ybounds[0],zbounds[0]);
    this->PlaneSource->SetPoint2(xbounds[0],ybounds[1],zbounds[0]);
    }
  else if ( i == 0 ) //YZ, x-normal
    {
    this->PlaneSource->SetOrigin(xbounds[0],ybounds[0],zbounds[0]);
    this->PlaneSource->SetPoint1(xbounds[0],ybounds[1],zbounds[0]);
    this->PlaneSource->SetPoint2(xbounds[0],ybounds[0],zbounds[1]);
    }
  else  //ZX, y-normal
    {
    this->PlaneSource->SetOrigin(xbounds[0],ybounds[0],zbounds[0]);
    this->PlaneSource->SetPoint1(xbounds[0],ybounds[0],zbounds[1]);
    this->PlaneSource->SetPoint2(xbounds[1],ybounds[0],zbounds[0]);
    }

  this->UpdatePlane();
  this->BuildRepresentation();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetInputConnection(vtkAlgorithmOutput* aout)
{
  this->Superclass::SetInputConnection(aout);

  this->ImageData = vtkImageData::SafeDownCast(
    aout->GetProducer()->GetOutputDataObject(
      aout->GetIndex()));

  if( !this->ImageData )
    {
    // If NULL is passed, remove any reference that Reslice had
    // on the old ImageData
    //
    this->Reslice->SetInputData(NULL);
    return;
    }

  this->Reslice->SetInputConnection(aout);
  int interpolate = this->ResliceInterpolate;
  this->ResliceInterpolate = -1; // Force change
  this->SetResliceInterpolate(interpolate);

  this->Texture->SetInputConnection(this->Reslice->GetOutputPort());
  this->Texture->SetInterpolate(this->TextureInterpolate);

  this->SetPlaneOrientation(this->PlaneOrientation);
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::UpdatePlane()
{
  if ( !this->Reslice || !this->ImageData )
    {
    return;
    }

  vtkAlgorithm* inpAlg = this->Reslice->GetInputAlgorithm();
  inpAlg->UpdateInformation();
  vtkInformation* outInfo = inpAlg->GetOutputInformation(0);

  double bounds[6];
  this->FindPlaneBounds(outInfo, bounds);

  double spacing[3];
  outInfo->Get(vtkDataObject::SPACING(), spacing);

  //setup the clip bounds
  this->UpdateClipBounds(bounds, spacing);

  double planeCenter[3];
  this->PlaneSource->GetCenter(planeCenter);

  for (int i = 0; i < 3; i++ )
    {
    // Force the plane to lie within the true image bounds along its normal
    //
    if ( planeCenter[i] > bounds[2*i+1] )
      {
      planeCenter[i] = bounds[2*i+1];
      }
    else if ( planeCenter[i] < bounds[2*i] )
      {
      planeCenter[i] = bounds[2*i];
      }
    }

  this->PlaneSource->SetCenter(planeCenter);

  double planeAxis1[3];
  double planeAxis2[3];

  this->GetVector1(planeAxis1);
  this->GetVector2(planeAxis2);

  // The x,y dimensions of the plane
  //
  double planeSizeX = vtkMath::Normalize(planeAxis1);
  double planeSizeY = vtkMath::Normalize(planeAxis2);

  double normal[3];
  this->PlaneSource->GetNormal(normal);

  // Generate the slicing matrix
  //

  this->ResliceAxes->Identity();
  for (int i = 0; i < 3; i++ )
     {
     this->ResliceAxes->SetElement(0,i,planeAxis1[i]);
     this->ResliceAxes->SetElement(1,i,planeAxis2[i]);
     this->ResliceAxes->SetElement(2,i,normal[i]);
     }

  double planeOrigin[4];
  this->PlaneSource->GetOrigin(planeOrigin);

  planeOrigin[3] = 1.0;

  this->ResliceAxes->Transpose();
  this->ResliceAxes->SetElement(0,3,planeOrigin[0]);
  this->ResliceAxes->SetElement(1,3,planeOrigin[1]);
  this->ResliceAxes->SetElement(2,3,planeOrigin[2]);

  this->Reslice->SetResliceAxes(this->ResliceAxes);

  double spacingX = fabs(2*planeAxis1[0]*spacing[0])+
                    fabs(2*planeAxis1[1]*spacing[1])+
                    fabs(2*planeAxis1[2]*spacing[2]);

  double spacingY = fabs(2*planeAxis2[0]*spacing[0])+
                    fabs(2*planeAxis2[1]*spacing[1])+
                    fabs(2*planeAxis2[2]*spacing[2]);


  // Pad extent up to a power of two for efficient texture mapping

  // make sure we're working with valid values
  double realExtentX = ( spacingX == 0 ) ? VTK_INT_MAX : planeSizeX / spacingX;

  int extentX;
  // Sanity check the input data:
  // * if realExtentX is too large, extentX will wrap
  // * if spacingX is 0, things will blow up.
  if (realExtentX > (VTK_INT_MAX >> 1))
    {
    vtkErrorMacro(<<"Invalid X extent: " << realExtentX);
    extentX = 0;
    }
  else
    {
    extentX = 1;
    while (extentX < realExtentX)
      {
      extentX = extentX << 1;
      }
    }

  // make sure extentY doesn't wrap during padding
  double realExtentY = ( spacingY == 0 ) ? VTK_INT_MAX : planeSizeY / spacingY;

  int extentY;
  if (realExtentY > (VTK_INT_MAX >> 1))
    {
    vtkErrorMacro(<<"Invalid Y extent: " << realExtentY);
    extentY = 0;
    }
  else
    {
    extentY = 1;
    while (extentY < realExtentY)
      {
      extentY = extentY << 2;
      }
    }

  double outputSpacingX = (planeSizeX == 0) ? 1.0 : planeSizeX/extentX;
  double outputSpacingY = (planeSizeY == 0) ? 1.0 : planeSizeY/extentY;

  // std::cout << "outputSpacingX: " << outputSpacingX << std::endl;
  // std::cout << "outputSpacingY: " << outputSpacingX << std::endl;
  // std::cout << "extentX: " << extentX << std::endl;
  // std::cout << "extentY: " << extentY << std::endl;
  // std::cout <<"IsTranslucent: " << this->Texture->IsTranslucent() << std::endl;


  this->Reslice->SetOutputSpacing(outputSpacingX, outputSpacingY, 1);
  this->Reslice->SetOutputOrigin(0.5*outputSpacingX, 0.5*outputSpacingY, 0);
  this->Reslice->SetOutputExtent(0, extentX-1, 0, extentY-1, 0, 0);
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::FindPlaneBounds(vtkInformation* outInfo,
                                               double bounds[6])
{
  // Calculate appropriate pixel spacing for the reslicing
  //
  double spacing[3];
  outInfo->Get(vtkDataObject::SPACING(), spacing);
  double origin[3];
  outInfo->Get(vtkDataObject::ORIGIN(), origin);
  int extent[6];
  outInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), extent);

  int i;

  double orig_bounds[] = {origin[0] + spacing[0]*extent[0], //xmin
                          origin[0] + spacing[0]*extent[1], //xmax
                          origin[1] + spacing[1]*extent[2], //ymin
                          origin[1] + spacing[1]*extent[3], //ymax
                          origin[2] + spacing[2]*extent[4], //zmin
                          origin[2] + spacing[2]*extent[5]};//zmax

  for ( i = 0; i <= 4; i += 2 ) // reverse bounds if necessary
    {
    if ( orig_bounds[i] > orig_bounds[i+1] )
      {
      double t = orig_bounds[i+1];
      orig_bounds[i+1] = orig_bounds[i];
      orig_bounds[i] = t;
      }
    }

  //update the bounds
  bounds[0]=orig_bounds[0];
  bounds[1]=orig_bounds[1];
  bounds[2]=orig_bounds[2];
  bounds[3]=orig_bounds[3];
  bounds[4]=orig_bounds[4];
  bounds[5]=orig_bounds[5];
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::UpdateClipBounds(double bounds[6],
                                                double spacing[3])
{
  //todo: we need to cache this so we don't redo this every frame

  //setup custom clip planes for the texture mapper so that it doesn't
  //draw outside the box
  vtkNew<vtkPlaneCollection> clippingPlanes;

  //we push the bounds out by two voxels by using the spacing
  double clip_bounds[6] = { bounds[0] - (2 *spacing[0]),
                            bounds[1] + (2 *spacing[0]),
                            bounds[2] - (2 *spacing[1]),
                            bounds[3] + (2 *spacing[1]),
                            bounds[4] - (2 *spacing[2]),
                            bounds[5] + (2 *spacing[2])
                          };

  vtkNew<vtkPlane> minXPlane;
  minXPlane->SetOrigin(clip_bounds[0],clip_bounds[2],clip_bounds[4]);
  minXPlane->SetNormal(1,0,0); //clip everything on low x

  vtkNew<vtkPlane> maxXPlane;
  maxXPlane->SetOrigin(clip_bounds[1],clip_bounds[3],clip_bounds[5]);
  maxXPlane->SetNormal(-1,0,0); //clip everything on high x

  vtkNew<vtkPlane> minYPlane;
  minYPlane->SetOrigin(clip_bounds[0],clip_bounds[2],clip_bounds[4]);
  minYPlane->SetNormal(0,1,0); //clip everything on low y

  vtkNew<vtkPlane> maxYPlane;
  maxYPlane->SetOrigin(clip_bounds[1],clip_bounds[3],clip_bounds[5]);
  maxYPlane->SetNormal(0,-1,0); //clip everything on high y

  vtkNew<vtkPlane> minZPlane;
  minZPlane->SetOrigin(clip_bounds[0],clip_bounds[2],clip_bounds[4]);
  minZPlane->SetNormal(0,0,1); //clip everything on low y

  vtkNew<vtkPlane> maxZPlane;
  maxZPlane->SetOrigin(clip_bounds[1],clip_bounds[3],clip_bounds[5]);
  maxZPlane->SetNormal(0,0,-1); //clip everything on high y

  clippingPlanes->AddItem(minXPlane.GetPointer());
  clippingPlanes->AddItem(maxXPlane.GetPointer());

  clippingPlanes->AddItem(minYPlane.GetPointer());
  clippingPlanes->AddItem(maxYPlane.GetPointer());

  clippingPlanes->AddItem(minZPlane.GetPointer());
  clippingPlanes->AddItem(maxZPlane.GetPointer());

  this->TexturePlaneActor->GetMapper()->SetClippingPlanes(
                                              clippingPlanes.GetPointer());
}

//----------------------------------------------------------------------------
vtkImageData* vtkColorImagePlaneWidget::GetResliceOutput()
{
  if ( ! this->Reslice )
    {
    return 0;
    }
  return this->Reslice->GetOutput();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetPicker(vtkAbstractPropPicker* picker)
{
  // we have to have a picker for slice motion, window level and cursor to work
  if (this->PlanePicker != picker)
    {
    // to avoid destructor recursion
    vtkAbstractPropPicker *temp = this->PlanePicker;
    this->PlanePicker = picker;
    if (temp != 0)
      {
      temp->UnRegister(this);
      }

    int delPicker = 0;
    if (this->PlanePicker == 0)
      {
      this->PlanePicker = vtkCellPicker::New();
      vtkCellPicker::SafeDownCast(this->PlanePicker)->SetTolerance(0.005);
      delPicker = 1;
      }

    this->PlanePicker->Register(this);
    this->PlanePicker->AddPickList(this->TexturePlaneActor);
    this->PlanePicker->PickFromListOn();

    if ( delPicker )
      {
      this->PlanePicker->Delete();
      }
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetResliceInterpolate(int i)
{
  if ( this->ResliceInterpolate == i )
    {
    return;
    }
  this->ResliceInterpolate = i;
  this->Modified();

  if ( !this->Reslice )
    {
    return;
    }

  if ( i == VTK_NEAREST_RESLICE )
    {
    this->Reslice->SetInterpolationModeToNearestNeighbor();
    }
  else if ( i == VTK_LINEAR_RESLICE)
    {
    this->Reslice->SetInterpolationModeToLinear();
    }
  else
    {
    this->Reslice->SetInterpolationModeToCubic();
    }
  this->Texture->SetInterpolate(this->TextureInterpolate);
}

//----------------------------------------------------------------------------
vtkScalarsToColors* vtkColorImagePlaneWidget::CreateDefaultLookupTable()
{
  vtkLookupTable* lut = vtkLookupTable::New();
  lut->Register(this);
  lut->Delete();
  lut->SetNumberOfColors( 256);
  lut->SetHueRange( 0, 0);
  lut->SetSaturationRange( 0, 0);
  lut->SetValueRange( 0 ,1);
  lut->SetAlphaRange( 1, 1);
  lut->Build();
  return lut;
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetLookupTable(vtkScalarsToColors* table)
{
  if (this->LookupTable != table)
    {
    // to avoid destructor recursion
    vtkScalarsToColors *temp = this->LookupTable;
    this->LookupTable = table;
    if (temp != 0)
      {
      temp->UnRegister(this);
      }
    if (this->LookupTable != 0)
      {
      this->LookupTable->Register(this);
      }
    else  //create a default lut
      {
      this->LookupTable = this->CreateDefaultLookupTable();
      }
    }

  this->Texture->SetLookupTable(this->LookupTable);

  if(this->ImageData)
    {
    double range[2];
    this->ImageData->GetScalarRange(range);

    this->LookupTable->Build();
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetSlicePosition(double position)
{
  double amount = 0.0;
  double planeOrigin[3];
  this->PlaneSource->GetOrigin( planeOrigin );

  if ( this->PlaneOrientation == 2 ) // z axis
    {
    amount = position - planeOrigin[2];
    }
  else if ( this->PlaneOrientation == 0 ) // x axis
    {
    amount = position - planeOrigin[0];
    }
  else if ( this->PlaneOrientation == 1 )  //y axis
    {
    amount = position - planeOrigin[1];
    }
  else
    {
    vtkGenericWarningMacro("only works for ortho planes: set plane orientation first");
    return;
    }

  this->PlaneSource->Push( amount );
  this->UpdatePlane();
  this->BuildRepresentation();
  this->Modified();
}

//----------------------------------------------------------------------------
double vtkColorImagePlaneWidget::GetSlicePosition()
{
  double planeOrigin[3];
  this->PlaneSource->GetOrigin( planeOrigin);

  if ( this->PlaneOrientation == 2 )
    {
    return planeOrigin[2];
    }
  else if ( this->PlaneOrientation == 1 )
    {
    return planeOrigin[1];
    }
  else if ( this->PlaneOrientation == 0 )
    {
    return planeOrigin[0];
    }
  else
    {
    vtkGenericWarningMacro("only works for ortho planes: set plane orientation first");
    }

  return 0.0;
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetSliceIndex(int index)
{
  if ( !this->Reslice )
    {
      return;
    }
  if ( !this->ImageData )
    {
    return;
    }
  vtkAlgorithm* inpAlg = this->Reslice->GetInputAlgorithm();
  inpAlg->UpdateInformation();
  vtkInformation* outInfo = inpAlg->GetOutputInformation(0);
  double origin[3];
  outInfo->Get(vtkDataObject::ORIGIN(), origin);
  double spacing[3];
  outInfo->Get(vtkDataObject::SPACING(), spacing);
  double planeOrigin[3];
  this->PlaneSource->GetOrigin(planeOrigin);
  double pt1[3];
  this->PlaneSource->GetPoint1(pt1);
  double pt2[3];
  this->PlaneSource->GetPoint2(pt2);

  if ( this->PlaneOrientation == 2 )
    {
    planeOrigin[2] = origin[2] + index*spacing[2];
    pt1[2] = planeOrigin[2];
    pt2[2] = planeOrigin[2];
    }
  else if ( this->PlaneOrientation == 1 )
    {
    planeOrigin[1] = origin[1] + index*spacing[1];
    pt1[1] = planeOrigin[1];
    pt2[1] = planeOrigin[1];
    }
  else if ( this->PlaneOrientation == 0 )
    {
    planeOrigin[0] = origin[0] + index*spacing[0];
    pt1[0] = planeOrigin[0];
    pt2[0] = planeOrigin[0];
    }
  else
    {
    vtkGenericWarningMacro("only works for ortho planes: set plane orientation first");
    return;
    }

  this->PlaneSource->SetOrigin(planeOrigin);
  this->PlaneSource->SetPoint1(pt1);
  this->PlaneSource->SetPoint2(pt2);
  this->UpdatePlane();
  this->BuildRepresentation();
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkColorImagePlaneWidget::GetSliceIndex()
{
  if ( ! this->Reslice )
    {
    return 0;
    }
  if ( ! this->ImageData )
    {
    return 0;
    }
  vtkAlgorithm* inpAlg = this->Reslice->GetInputAlgorithm();
  inpAlg->UpdateInformation();
  vtkInformation* outInfo = inpAlg->GetOutputInformation(0);
  double origin[3];
  outInfo->Get(vtkDataObject::ORIGIN(), origin);
  double spacing[3];
  outInfo->Get(vtkDataObject::SPACING(), spacing);
  double planeOrigin[3];
  this->PlaneSource->GetOrigin(planeOrigin);

  if ( this->PlaneOrientation == 2 )
    {
    return vtkMath::Round((planeOrigin[2]-origin[2])/spacing[2]);
    }
  else if ( this->PlaneOrientation == 1 )
    {
    return vtkMath::Round((planeOrigin[1]-origin[1])/spacing[1]);
    }
  else if ( this->PlaneOrientation == 0 )
    {
    return vtkMath::Round((planeOrigin[0]-origin[0])/spacing[0]);
    }
  else
    {
    vtkGenericWarningMacro("only works for ortho planes: set plane orientation first");
    }

  return 0;
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::ActivateMargins(int i)
{

  if( !this->CurrentRenderer )
    {
    return;
    }

  if( i == 0 )
    {
    this->MarginActor->VisibilityOff();
    }
  else
    {
    this->MarginActor->VisibilityOn();
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetOrigin(double x, double y, double z)
{
  this->PlaneSource->SetOrigin(x,y,z);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetOrigin(double xyz[3])
{
  this->PlaneSource->SetOrigin(xyz);
  this->Modified();
}

//----------------------------------------------------------------------------
double* vtkColorImagePlaneWidget::GetOrigin()
{
  return this->PlaneSource->GetOrigin();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetOrigin(double xyz[3])
{
  this->PlaneSource->GetOrigin(xyz);
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetPoint1(double x, double y, double z)
{
  this->PlaneSource->SetPoint1(x,y,z);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetPoint1(double xyz[3])
{
  this->PlaneSource->SetPoint1(xyz);
  this->Modified();
}

//----------------------------------------------------------------------------
double* vtkColorImagePlaneWidget::GetPoint1()
{
  return this->PlaneSource->GetPoint1();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetPoint1(double xyz[3])
{
  this->PlaneSource->GetPoint1(xyz);
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetPoint2(double x, double y, double z)
{
  this->PlaneSource->SetPoint2(x,y,z);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::SetPoint2(double xyz[3])
{
  this->PlaneSource->SetPoint2(xyz);
  this->Modified();
}

//----------------------------------------------------------------------------
double* vtkColorImagePlaneWidget::GetPoint2()
{
  return this->PlaneSource->GetPoint2();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetPoint2(double xyz[3])
{
  this->PlaneSource->GetPoint2(xyz);
}

//----------------------------------------------------------------------------
double* vtkColorImagePlaneWidget::GetCenter()
{
  return this->PlaneSource->GetCenter();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetCenter(double xyz[3])
{
  this->PlaneSource->GetCenter(xyz);
}

//----------------------------------------------------------------------------
double* vtkColorImagePlaneWidget::GetNormal()
{
  return this->PlaneSource->GetNormal();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetNormal(double xyz[3])
{
  this->PlaneSource->GetNormal(xyz);
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetPolyData(vtkPolyData *pd)
{
  pd->ShallowCopy(this->PlaneSource->GetOutput());
}

//----------------------------------------------------------------------------
vtkPolyDataAlgorithm *vtkColorImagePlaneWidget::GetPolyDataAlgorithm()
{
  return this->PlaneSource;
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::UpdatePlacement(void)
{
  this->UpdatePlane();
  this->UpdateMargins();

  this->Texture->Update();
  this->BuildRepresentation();
}

//----------------------------------------------------------------------------
vtkTexture* vtkColorImagePlaneWidget::GetTexture()
{
  return this->Texture;
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetVector1(double v1[3])
{
  double* p1 = this->PlaneSource->GetPoint1();
  double* o =  this->PlaneSource->GetOrigin();
  v1[0] = p1[0] - o[0];
  v1[1] = p1[1] - o[1];
  v1[2] = p1[2] - o[2];
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GetVector2(double v2[3])
{
  double* p2 = this->PlaneSource->GetPoint2();
  double* o =  this->PlaneSource->GetOrigin();
  v2[0] = p2[0] - o[0];
  v2[1] = p2[1] - o[1];
  v2[2] = p2[2] - o[2];
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::AdjustState()
{
  double v1[3];
  this->GetVector1(v1);
  double v2[3];
  this->GetVector2(v2);

  double planeSize1 = vtkMath::Normalize(v1);
  double planeSize2 = vtkMath::Normalize(v2);
  double* planeOrigin = this->PlaneSource->GetOrigin();

  double ppo[3] = {this->LastPickPosition[0] - planeOrigin[0],
                  this->LastPickPosition[1] - planeOrigin[1],
                  this->LastPickPosition[2] - planeOrigin[2] };

  double x2D = vtkMath::Dot(ppo,v1);
  double y2D = vtkMath::Dot(ppo,v2);

  if ( x2D > planeSize1 ) { x2D = planeSize1; }
  else if ( x2D < 0.0 ) { x2D = 0.0; }
  if ( y2D > planeSize2 ) { y2D = planeSize2; }
  else if ( y2D < 0.0 ) { y2D = 0.0; }

  // Divide plane into three zones for different user interactions:
  // four corners -- spin around the plane's normal at its center
  // four edges   -- rotate around one of the plane's axes at its center
  // center area  -- push
  //
  double marginX = planeSize1 * this->MarginSizeX;
  double marginY = planeSize2 * this->MarginSizeY;

  double x0 = marginX;
  double y0 = marginY;
  double x1 = planeSize1 - marginX;
  double y1 = planeSize2 - marginY;

  if ( x2D < x0  )       // left margin
    {
    if (y2D < y0)        // bottom left corner
      {
      this->MarginSelectMode =  0;
      }
    else if (y2D > y1)   // top left corner
      {
      this->MarginSelectMode =  3;
      }
    else                 // left edge
      {
      this->MarginSelectMode =  4;
      }
    }
  else if ( x2D > x1 )   // right margin
    {
    if (y2D < y0)        // bottom right corner
      {
      this->MarginSelectMode =  1;
      }
    else if (y2D > y1)   // top right corner
      {
      this->MarginSelectMode =  2;
      }
    else                 // right edge
      {
      this->MarginSelectMode =  5;
      }
    }
  else                   // middle or on the very edge
    {
    if (y2D < y0)        // bottom edge
      {
      this->MarginSelectMode =  6;
      }
    else if (y2D > y1)   // top edge
      {
      this->MarginSelectMode =  7;
      }
    else                 // central area
      {
      this->MarginSelectMode =  8;
      }
    }


  if (this->MarginSelectMode >= 0 && this->MarginSelectMode < 4)
    {
    this->State = vtkColorImagePlaneWidget::Pushing;
    return;
    }
  else if (this->MarginSelectMode == 8)
    {
    this->State = vtkColorImagePlaneWidget::Pushing;
    return;
    }
  else
    {
    this->State = vtkColorImagePlaneWidget::Rotating;
    }

  double *raPtr = 0;
  double *rvPtr = 0;
  double rvfac = 1.0;
  double rafac = 1.0;

  switch ( this->MarginSelectMode )
    {
     // left bottom corner
    case 0: raPtr = v2; rvPtr = v1; rvfac = -1.0; rafac = -1.0; break;
     // right bottom corner
    case 1: raPtr = v2; rvPtr = v1;               rafac = -1.0; break;
     // right top corner
    case 2: raPtr = v2; rvPtr = v1;               break;
     // left top corner
    case 3: raPtr = v2; rvPtr = v1; rvfac = -1.0; break;
    case 4: raPtr = v2; rvPtr = v1; rvfac = -1.0; break; // left
    case 5: raPtr = v2; rvPtr = v1;               break; // right
    case 6: raPtr = v1; rvPtr = v2; rvfac = -1.0; break; // bottom
    case 7: raPtr = v1; rvPtr = v2;               break; // top
    default: raPtr = v1; rvPtr = v2; break;
    }

  for (int i = 0; i < 3; i++)
    {
    this->RotateAxis[i] = *raPtr++ * rafac;
    this->RadiusVector[i] = *rvPtr++ * rvfac;
    }
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::Rotate(double X, double Y,
                                      double *p1, double *p2, double *vpn)
{
  double v[3]; //vector of motion
  double axis[3]; //axis of rotation
  double theta; //rotation angle

  // mouse motion vector in world space
  v[0] = p2[0] - p1[0];
  v[1] = p2[1] - p1[1];
  v[2] = p2[2] - p1[2];

  double *origin = this->PlaneSource->GetOrigin();
  double *normal = this->PlaneSource->GetNormal();

  // Create axis of rotation and angle of rotation
  vtkMath::Cross(vpn,v,axis);
  if ( vtkMath::Normalize(axis) == 0.0 )
    {
    return;
    }
  int *const int_lastPos = this->Interactor->GetLastEventPosition();
  const double lastPos[2]={int_lastPos[0],int_lastPos[1]};

  int *size = this->CurrentRenderer->GetSize();
  double l2 = (X-lastPos[0])*(X-lastPos[0]) + (Y-lastPos[1])*(Y-lastPos[1]);
  theta = 360.0 * sqrt(l2/(size[0]*size[0]+size[1]*size[1]));

  //Manipulate the transform to reflect the rotation
  this->Transform->Identity();
  this->Transform->Translate(origin[0],origin[1],origin[2]);
  this->Transform->RotateWXYZ(theta,axis);
  this->Transform->Translate(-origin[0],-origin[1],-origin[2]);

  //Set the new normal
  double new_normal[3];
  this->Transform->TransformNormal(normal,new_normal);
  this->PlaneSource->SetNormal(new_normal);
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GeneratePlaneOutline()
{
  vtkPoints* points   = vtkPoints::New(VTK_DOUBLE);
  points->SetNumberOfPoints(4);
  int i;
  for (i = 0; i < 4; i++)
    {
    points->SetPoint(i,0.0,0.0,0.0);
    }

  vtkCellArray *cells = vtkCellArray::New();
  cells->Allocate(cells->EstimateSize(4,2));
  vtkIdType pts[2];
  pts[0] = 3; pts[1] = 2;       // top edge
  cells->InsertNextCell(2,pts);
  pts[0] = 0; pts[1] = 1;       // bottom edge
  cells->InsertNextCell(2,pts);
  pts[0] = 0; pts[1] = 3;       // left edge
  cells->InsertNextCell(2,pts);
  pts[0] = 1; pts[1] = 2;       // right edge
  cells->InsertNextCell(2,pts);

  this->PlaneOutlinePolyData->SetPoints(points);
  points->Delete();
  this->PlaneOutlinePolyData->SetLines(cells);
  cells->Delete();

  vtkPolyDataMapper* planeOutlineMapper = vtkPolyDataMapper::New();
  planeOutlineMapper->SetInputData( this->PlaneOutlinePolyData );
  planeOutlineMapper->SetResolveCoincidentTopologyToPolygonOffset();

  this->PlaneOutlineActor->SetMapper(planeOutlineMapper);
  this->PlaneOutlineActor->PickableOff();
  planeOutlineMapper->Delete();
}

//------------------------------------------------------------------------------
void vtkColorImagePlaneWidget::RegisterPickers()
{
  this->Interactor->GetPickingManager()->AddPicker(this->PlanePicker, this);
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GenerateTexturePlane()
{
  this->SetResliceInterpolate(this->ResliceInterpolate);

  this->LookupTable = this->CreateDefaultLookupTable();

  vtkNew<vtkPolyDataMapper> texturePlaneMapper;
  texturePlaneMapper->SetInputConnection(
    this->PlaneSource->GetOutputPort());

  this->Texture->SetQualityTo32Bit();
  this->Texture->MapColorScalarsThroughLookupTableOn();
  this->Texture->SetInterpolate(this->TextureInterpolate);
  this->Texture->RepeatOff();
  this->Texture->SetLookupTable(this->LookupTable);

  this->TexturePlaneActor->SetMapper(texturePlaneMapper.GetPointer());
  this->TexturePlaneActor->SetTexture(this->Texture);
  this->TexturePlaneActor->PickableOn();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::GenerateMargins()
{
  // Construct initial points
  vtkPoints* points = vtkPoints::New(VTK_DOUBLE);
  points->SetNumberOfPoints(8);
  int i;
  for (i = 0; i < 8; i++)
    {
    points->SetPoint(i,0.0,0.0,0.0);
    }

  vtkCellArray *cells = vtkCellArray::New();
  cells->Allocate(cells->EstimateSize(4,2));
  vtkIdType pts[2];
  pts[0] = 0; pts[1] = 1;       // top margin
  cells->InsertNextCell(2,pts);
  pts[0] = 2; pts[1] = 3;       // bottom margin
  cells->InsertNextCell(2,pts);
  pts[0] = 4; pts[1] = 5;       // left margin
  cells->InsertNextCell(2,pts);
  pts[0] = 6; pts[1] = 7;       // right margin
  cells->InsertNextCell(2,pts);

  this->MarginPolyData->SetPoints(points);
  points->Delete();
  this->MarginPolyData->SetLines(cells);
  cells->Delete();

  vtkPolyDataMapper* marginMapper = vtkPolyDataMapper::New();
  marginMapper->SetInputData(this->MarginPolyData);
  marginMapper->SetResolveCoincidentTopologyToPolygonOffset();
  this->MarginActor->SetMapper(marginMapper);
  this->MarginActor->PickableOff();
  this->MarginActor->VisibilityOff();
  marginMapper->Delete();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::UpdateMargins()
{
  double v1[3];
  this->GetVector1(v1);
  double v2[3];
  this->GetVector2(v2);
  double o[3];
  this->PlaneSource->GetOrigin(o);
  double p1[3];
  this->PlaneSource->GetPoint1(p1);
  double p2[3];
  this->PlaneSource->GetPoint2(p2);

  double a[3];
  double b[3];
  double c[3];
  double d[3];

  double s = this->MarginSizeX;
  double t = this->MarginSizeY;

  int i;
  for ( i = 0; i < 3; i++)
    {
    a[i] = o[i] + v2[i]*(1-t);
    b[i] = p1[i] + v2[i]*(1-t);
    c[i] = o[i] + v2[i]*t;
    d[i] = p1[i] + v2[i]*t;
    }

  vtkPoints* marginPts = this->MarginPolyData->GetPoints();

  marginPts->SetPoint(0,a);
  marginPts->SetPoint(1,b);
  marginPts->SetPoint(2,c);
  marginPts->SetPoint(3,d);

  for ( i = 0; i < 3; i++)
    {
    a[i] = o[i] + v1[i]*s;
    b[i] = p2[i] + v1[i]*s;
    c[i] = o[i] + v1[i]*(1-s);
    d[i] = p2[i] + v1[i]*(1-s);
    }

  marginPts->SetPoint(4,a);
  marginPts->SetPoint(5,b);
  marginPts->SetPoint(6,c);
  marginPts->SetPoint(7,d);

  this->MarginPolyData->Modified();
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::Scale(double *p1, double *p2,
                                int vtkNotUsed(X), int Y)
{
  // Get the motion vector
  //
  double v[3];
  v[0] = p2[0] - p1[0];
  v[1] = p2[1] - p1[1];
  v[2] = p2[2] - p1[2];

  double *o = this->PlaneSource->GetOrigin();
  double *pt1 = this->PlaneSource->GetPoint1();
  double *pt2 = this->PlaneSource->GetPoint2();
  double* center = this->PlaneSource->GetCenter();

  // Compute the scale factor
  //
  double sf = vtkMath::Norm(v) /
    sqrt(vtkMath::Distance2BetweenPoints(pt1,pt2));
  if ( Y > this->Interactor->GetLastEventPosition()[1] )
    {
    sf = 1.0 + sf;
    }
  else
    {
    sf = 1.0 - sf;
    }

  // Move the corner points
  //
  double origin[3], point1[3], point2[3];

  for (int i=0; i<3; i++)
    {
    origin[i] = sf * (o[i] - center[i]) + center[i];
    point1[i] = sf * (pt1[i] - center[i]) + center[i];
    point2[i] = sf * (pt2[i] - center[i]) + center[i];
    }

  this->PlaneSource->SetOrigin(origin);
  this->PlaneSource->SetPoint1(point1);
  this->PlaneSource->SetPoint2(point2);
}


