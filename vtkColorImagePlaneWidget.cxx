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
#include "vtkConeSource.h"
#include "vtkImageData.h"
#include "vtkImageReslice.h"
#include "vtkInformation.h"
#include "vtkLineSource.h"
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
#include "vtkSphereSource.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkTexture.h"
#include "vtkTransform.h"

vtkStandardNewMacro(vtkColorImagePlaneWidget);

namespace detail
{

  //goal is to make an extent value that is a power of 2 and
  //is greater or equal size to the real extent
  int make_extent(double planeSize, double spacing)
  {
  // make sure we're working with valid values
  double realExtent = ( spacing == 0 ) ? VTK_INT_MAX : planeSize / spacing;

  int extent = 0;
  // Sanity check the input data:
  // * if realExtent is too large, extent will wrap
  // * if spacing is 0, things will blow up.
  if (realExtent < (VTK_INT_MAX >> 1))
    {
    extent = 1;
    //compute the largest power of 2 that is greater or equal to realExtent
    while (extent < realExtent)
      { extent = extent << 1; }
    }
  return extent;
  }
}

vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, PlaneProperty, vtkProperty);
vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, SelectedPlaneProperty, vtkProperty);
vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, ArrowProperty, vtkProperty);
vtkCxxSetObjectMacro(vtkColorImagePlaneWidget, SelectedArrowProperty, vtkProperty);
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

  // Represent the positioning arrow
  //
  this->LineSource = vtkLineSource::New();
  this->LineActor = vtkActor::New();

  this->ConeSource = vtkConeSource::New();
  this->ConeActor = vtkActor::New();

  this->LineSource2 = vtkLineSource::New();
  this->LineActor2 = vtkActor::New();

  this->ConeSource2 = vtkConeSource::New();
  this->ConeActor2 = vtkActor::New();

  this->Sphere = vtkSphereSource::New();
  this->SphereActor = vtkActor::New();


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
  this->GeneratePlaneOutline();
  this->PlaceWidget(bounds);
  this->GenerateTexturePlane();
  this->GenerateArrow();

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
  this->ArrowProperty         = 0;
  this->SelectedArrowProperty = 0;
  this->TexturePlaneProperty  = 0;
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

  if ( this->ArrowProperty )
    {
    this->ArrowProperty->Delete();
    }

  if ( this->SelectedArrowProperty )
    {
    this->SelectedArrowProperty->Delete();
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

  //delete everything related to the arrow
  this->LineSource->Delete();
  this->LineActor->Delete();
  this->ConeSource->Delete();
  this->ConeActor->Delete();
  this->LineSource2->Delete();
  this->LineActor2->Delete();
  this->ConeSource2->Delete();
  this->ConeActor2->Delete();
  this->Sphere->Delete();
  this->SphereActor->Delete();
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

    // add the default arrow properties
    this->CurrentRenderer->AddViewProp(this->LineActor);
    this->CurrentRenderer->AddViewProp(this->ConeActor);
    this->CurrentRenderer->AddViewProp(this->LineActor2);
    this->CurrentRenderer->AddViewProp(this->ConeActor2);
    this->CurrentRenderer->AddViewProp(this->SphereActor);

    this->LineActor->SetProperty(this->ArrowProperty);
    this->ConeActor->SetProperty(this->ArrowProperty);
    this->LineActor2->SetProperty(this->ArrowProperty);
    this->ConeActor2->SetProperty(this->ArrowProperty);
    this->SphereActor->SetProperty(this->ArrowProperty);

    this->TexturePlaneActor->PickableOn();
    this->LineActor->PickableOn();
    this->ConeActor->PickableOn();
    this->LineActor2->PickableOn();
    this->ConeActor2->PickableOn();
    this->SphereActor->PickableOn();


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

    //turn off the arrow
    this->CurrentRenderer->RemoveViewProp(this->LineActor);
    this->CurrentRenderer->RemoveViewProp(this->ConeActor);
    this->CurrentRenderer->RemoveViewProp(this->LineActor2);
    this->CurrentRenderer->RemoveViewProp(this->ConeActor2);
    this->CurrentRenderer->RemoveViewProp(this->SphereActor);

    this->TexturePlaneActor->PickableOff();
    this->LineActor->PickableOff();
    this->ConeActor->PickableOff();
    this->LineActor2->PickableOff();
    this->ConeActor2->PickableOff();
    this->SphereActor->PickableOff();

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
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::BuildRepresentation()
{
  this->PlaneSource->Update();
  double *origin = this->PlaneSource->GetOrigin();
  double *normal = this->PlaneSource->GetNormal();
  double *pt1 = this->PlaneSource->GetPoint1();
  double *pt2 = this->PlaneSource->GetPoint2();

  double x[3];
  x[0] = origin[0] + (pt1[0]-origin[0]) + (pt2[0]-origin[0]);
  x[1] = origin[1] + (pt1[1]-origin[1]) + (pt2[1]-origin[1]);
  x[2] = origin[2] + (pt1[2]-origin[2]) + (pt2[2]-origin[2]);

  vtkPoints* points = this->PlaneOutlinePolyData->GetPoints();
  points->SetPoint(0,origin);
  points->SetPoint(1,pt1);
  points->SetPoint(2,x);
  points->SetPoint(3,pt2);
  points->GetData()->Modified();
  this->PlaneOutlinePolyData->Modified();

  // Setup the plane normal
  double d = 1 ;
  if(this->ImageData)
    {
    d = this->ImageData->GetLength();
    }
  else
    {
    d = vtkMath::Distance2BetweenPoints(pt1,pt2);
    }

  //compute the center of the plane
  double center[3];
  center[0] = origin[0] + ((pt1[0]-origin[0]) + (pt2[0]-origin[0]))/2.0;
  center[1] = origin[1] + ((pt1[1]-origin[1]) + (pt2[1]-origin[1]))/2.0;
  center[2] = origin[2] + ((pt1[2]-origin[2]) + (pt2[2]-origin[2]))/2.0;

  double p1[3];
  p1[0] = center[0] + 0.30 * d * normal[0];
  p1[1] = center[1] + 0.30 * d * normal[1];
  p1[2] = center[2] + 0.30 * d * normal[2];

  this->LineSource->SetPoint1(center);
  this->LineSource->SetPoint2(p1);
  this->ConeSource->SetCenter(p1);
  this->ConeSource->SetDirection(normal);

  double p2[3];
  p2[0] = center[0] - 0.30 * d * normal[0];
  p2[1] = center[1] - 0.30 * d * normal[1];
  p2[2] = center[2] - 0.30 * d * normal[2];

  this->LineSource2->SetPoint1(center[0],center[1],center[2]);
  this->LineSource2->SetPoint2(p2);
  this->ConeSource2->SetCenter(p2);
  this->ConeSource2->SetDirection(normal[0],normal[1],normal[2]);

  // Set up the position handle
  this->Sphere->SetCenter(center[0],center[1],center[2]);

  this->UpdateArrowSize();
  std::cout << "BuildRepresentation" << std::endl;
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::UpdateArrowSize()
{
  //we only want to rescale once we have an active camera, otherwise the
  //initial arrow takes up the entire render window
  if ( !this->CurrentRenderer || !this->CurrentRenderer->GetActiveCamera() )
    {
    return;
    }

  //hard code the controls for now
  const double handleSize = 5.0;
  const double factor = 1.5;
  double* pos = this->Sphere->GetCenter();

  double lowerLeft[4];
  double upperRight[4];
  double focalPoint[4];

  this->ComputeWorldToDisplay(pos[0], pos[1], pos[2], focalPoint);
  const double z = focalPoint[2];

  double x = focalPoint[0] - handleSize/2.0;
  double y = focalPoint[1] - handleSize/2.0;
  this->ComputeDisplayToWorld(x,y,z,lowerLeft);

  x = focalPoint[0] + handleSize/2.0;
  y = focalPoint[1] + handleSize/2.0;
  this->ComputeDisplayToWorld(x,y,z,upperRight);

  double scaledRadius = factor;
  {
  double radius=0.0;
  for (int i=0; i<3; i++)
    {
    radius += (upperRight[i] - lowerLeft[i]) * (upperRight[i] - lowerLeft[i]);
    }
  scaledRadius = factor * (sqrt(radius) / 2.0);
  } //scope this so nobody uses radius


  this->ConeSource->SetHeight(2.0*scaledRadius);
  this->ConeSource->SetRadius(scaledRadius);
  this->ConeSource2->SetHeight(2.0*scaledRadius);
  this->ConeSource2->SetRadius(scaledRadius);
  this->Sphere->SetRadius(scaledRadius);

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
  bool stateFound = false;
  vtkAssemblyPath* path = this->GetAssemblyPath(X, Y, 0., this->PlanePicker);
  if ( path != NULL ) // Not picking this widget
    {
    vtkProp *prop = path->GetFirstNode()->GetViewProp();
    if ( prop == this->ConeActor || prop == this->LineActor ||
         prop == this->ConeActor2 || prop == this->LineActor2 )
      {
      this->State = vtkColorImagePlaneWidget::Rotating;
      this->HighlightPlane(1);
      this->HighlightArrow(1);
      stateFound = true;
      }
    else if(prop == this->TexturePlaneActor ||
            prop == this->SphereActor)
      {
      this->State = vtkColorImagePlaneWidget::Pushing;
      this->HighlightPlane(1);
      this->HighlightArrow(1);
      stateFound = true;
      }
    }
  if(!stateFound)
    {
    this->State = vtkColorImagePlaneWidget::Outside;
    this->HighlightPlane(0);
    this->HighlightArrow(0);
    return;
    }

  this->EventCallbackCommand->SetAbortFlag(1);
  this->StartInteraction();
  this->InvokeEvent(vtkCommand::StartInteractionEvent,0);
  this->Interactor->Render();
  return;
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
  this->HighlightArrow(0);

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
    this->UpdatePlacement();
    }
  else if ( this->State == vtkColorImagePlaneWidget::Rotating )
    {
    camera->GetViewPlaneNormal(vpn);
    this->Rotate(double(X), double(Y), prevPickPoint, pickPoint, vpn);
    this->UpdatePlacement();
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
  const double v[3] = {
                      p2[0] - p1[0],
                      p2[1] - p1[1],
                      p2[2] - p1[2]
                      };

  //take only the primary component of the motion vector
  double norm[3];
  this->PlaneSource->GetNormal(norm);
  const float dotV = vtkMath::Dot( v, norm  );

  this->PlaneSource->Push( dotV );
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::CreateDefaultProperties()
{
  if ( ! this->PlaneProperty )
    {
    //we are going to make sure the border is hidden
    //when not selected
    this->PlaneProperty = vtkProperty::New();
    this->PlaneProperty->SetOpacity(0);
    this->PlaneProperty->SetRepresentationToWireframe();
    }

  if ( ! this->SelectedPlaneProperty )
    {
    this->SelectedPlaneProperty = vtkProperty::New();
    this->SelectedPlaneProperty->SetAmbient(1);
    this->SelectedPlaneProperty->SetColor(0,1,0);
    this->SelectedPlaneProperty->SetRepresentationToWireframe();
    this->SelectedPlaneProperty->SetInterpolationToFlat();
    }

  if ( ! this->ArrowProperty )
    {
    this->ArrowProperty = vtkProperty::New();
    this->ArrowProperty->SetColor(1,1,1);
    this->ArrowProperty->SetLineWidth(2);
    }

  if ( ! this->SelectedArrowProperty )
    {
    this->SelectedArrowProperty = vtkProperty::New();
    this->ArrowProperty->SetLineWidth(2);
    this->SelectedArrowProperty->SetColor(0,0,1);
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
    this->LineSource->SetPoint2(0,1,0);
    }
  else if ( this->PlaneOrientation == 2 )
    {
    this->PlaneSource->SetOrigin(bounds[0],bounds[2],center[2]);
    this->PlaneSource->SetPoint1(bounds[1],bounds[2],center[2]);
    this->PlaneSource->SetPoint2(bounds[0],bounds[3],center[2]);
    this->LineSource->SetPoint2(0,0,1);
    }
  else //default or x-normal
    {
    this->PlaneSource->SetOrigin(center[0],bounds[2],bounds[4]);
    this->PlaneSource->SetPoint1(center[0],bounds[3],bounds[4]);
    this->PlaneSource->SetPoint2(center[0],bounds[2],bounds[5]);
    this->LineSource->SetPoint2(1,0,0);
    }

    this->LineSource->SetPoint1(this->PlaneSource->GetOrigin());

  this->UpdatePlacement();
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

  this->UpdatePlacement();
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

  double normal[3];
  this->PlaneSource->GetNormal(normal);

  double planeAxis1[3];
  double planeAxis2[3];

  this->GetVector1(planeAxis1);
  this->GetVector2(planeAxis2);

  // The x,y dimensions of the plane
  //
  double planeSizeX = vtkMath::Normalize(planeAxis1);
  double planeSizeY = vtkMath::Normalize(planeAxis2);

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

  double spacingX = fabs(planeAxis1[0]*spacing[0])+
                    fabs(planeAxis1[1]*spacing[1])+
                    fabs(planeAxis1[2]*spacing[2]);

  double spacingY = fabs(planeAxis2[0]*spacing[0])+
                    fabs(planeAxis2[1]*spacing[1])+
                    fabs(planeAxis2[2]*spacing[2]);

  // Pad extent up to a power of two for efficient texture mapping
  int extentX = detail::make_extent(planeSizeX,spacingX);
  int extentY = detail::make_extent(planeSizeY,spacingY);

  double outputSpacingX = (planeSizeX == 0) ? 1.0 : planeSizeX/extentX;
  double outputSpacingY = (planeSizeY == 0) ? 1.0 : planeSizeY/extentY;

  this->PlaneSource->SetCenter(planeCenter);
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
    this->PlanePicker->AddPickList(this->LineActor);
    this->PlanePicker->AddPickList(this->ConeActor);
    this->PlanePicker->AddPickList(this->LineActor2);
    this->PlanePicker->AddPickList(this->ConeActor2);
    this->PlanePicker->AddPickList(this->SphereActor);
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
  this->UpdatePlacement();
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
  this->UpdatePlacement();
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
void vtkColorImagePlaneWidget::GenerateArrow()
{
  // Create the + plane normal
  this->LineSource->SetResolution(1);
  vtkNew<vtkPolyDataMapper> lineMapper;
  lineMapper->SetInputConnection(this->LineSource->GetOutputPort());
  this->LineActor->SetMapper(lineMapper.GetPointer());

  this->ConeSource->SetResolution(12);
  this->ConeSource->SetAngle(25.0);
  vtkNew<vtkPolyDataMapper> coneMapper;
  coneMapper->SetInputConnection(this->ConeSource->GetOutputPort());
  this->ConeActor->SetMapper(coneMapper.GetPointer());

  // Create the - plane normal
  this->LineSource2->SetResolution(1);
  vtkNew<vtkPolyDataMapper> lineMapper2;
  lineMapper2->SetInputConnection(this->LineSource2->GetOutputPort());
  this->LineActor2->SetMapper(lineMapper2.GetPointer());


  this->ConeSource2->SetResolution(12);
  this->ConeSource2->SetAngle(25.0);
  vtkNew<vtkPolyDataMapper> coneMapper2;
  coneMapper2->SetInputConnection(this->ConeSource2->GetOutputPort());
  this->ConeActor2->SetMapper(coneMapper2.GetPointer());

  // Create the origin handle
  this->Sphere->SetThetaResolution(16);
  this->Sphere->SetPhiResolution(8);
  vtkNew<vtkPolyDataMapper> sphereMapper;
  sphereMapper->SetInputConnection(this->Sphere->GetOutputPort());
  this->SphereActor->SetMapper(sphereMapper.GetPointer());
}

//----------------------------------------------------------------------------
void vtkColorImagePlaneWidget::HighlightArrow(int highlight)
{
  if ( highlight )
    {
    this->LineActor->SetProperty(this->SelectedArrowProperty);
    this->ConeActor->SetProperty(this->SelectedArrowProperty);
    this->LineActor2->SetProperty(this->SelectedArrowProperty);
    this->ConeActor2->SetProperty(this->SelectedArrowProperty);
    this->SphereActor->SetProperty(this->SelectedArrowProperty);
    }
  else
    {
    this->LineActor->SetProperty(this->ArrowProperty);
    this->ConeActor->SetProperty(this->ArrowProperty);
    this->LineActor2->SetProperty(this->ArrowProperty);
    this->ConeActor2->SetProperty(this->ArrowProperty);
    this->SphereActor->SetProperty(this->ArrowProperty);
    }
}
