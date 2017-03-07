/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkVolumeScaleRepresentation.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkVolumeScaleRepresentation.h"
#include "vtkAxisActor2D.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointHandleRepresentation2D.h"
#include "vtkProperty2D.h"
#include "vtkRenderer.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkWindow.h"

#include "Utilities.h"

#include <sstream>

vtkStandardNewMacro(vtkVolumeScaleRepresentation)

//----------------------------------------------------------------------
vtkVolumeScaleRepresentation::vtkVolumeScaleRepresentation()
{
  double p[3] = { 0., 0., 0. };
  this->SetWorldPosition(p);

  // Disable the vtkBillboardTextActor3D, and replace it with our own
  // vtkTextActor.
  this->SetLabelVisibility(0);

  this->Label = vtkTextActor::New();
  this->Label->SetVisibility(true);
  this->Label->GetTextProperty()->SetColor(tomviz::offWhite);
  this->Label->SetPosition(0, 0);
  this->Label->GetTextProperty()->SetFontSize(40);
  this->Label->GetTextProperty()->SetJustificationToCentered();

  this->Update2DLabel();
}

//----------------------------------------------------------------------
vtkVolumeScaleRepresentation::~vtkVolumeScaleRepresentation()
{
  this->Label->Delete();
}

//----------------------------------------------------------------------
void vtkVolumeScaleRepresentation::BuildRepresentation()
{
  this->Superclass::BuildRepresentation();

  if (this->Label &&
      (this->Label->GetMTime() > this->BuildTime ||
       (this->Renderer && this->Renderer->GetVTKWindow() &&
        this->Renderer->GetVTKWindow()->GetMTime() > this->BuildTime))) {
    this->Update2DLabel();

    this->BuildTime.Modified();
  }
}

//----------------------------------------------------------------------
void vtkVolumeScaleRepresentation::GetActors2D(vtkPropCollection* pc)
{
  this->Label->GetActors(pc);
}

//----------------------------------------------------------------------
void vtkVolumeScaleRepresentation::Update2DLabel()
{
  {
    std::stringstream s;
    s << this->SideLength << " " << std::string(this->LengthUnit);
    this->Label->SetInput(s.str().c_str());
  }

  double labelPosition[3] = { 0., 0., 0. };
  if (this->Renderer) {
    this->GetWorldPosition(labelPosition);

    this->Renderer->SetWorldPoint(labelPosition);
    this->Renderer->WorldToDisplay();
    this->Renderer->GetDisplayPoint(labelPosition);

    double bbox[4];
    this->Label->GetBoundingBox(this->Renderer, bbox);
    labelPosition[1] -= 2. * (bbox[3] - bbox[2]);
  }

  this->Label->SetDisplayPosition(labelPosition[0], labelPosition[1]);
}

//----------------------------------------------------------------------
void vtkVolumeScaleRepresentation::SetRepresentationVisibility(bool choice)
{
  this->Superclass::SetHandleVisibility(choice);
  this->Label->SetVisibility(choice);
  this->Modified();
}

//----------------------------------------------------------------------
void vtkVolumeScaleRepresentation::ReleaseGraphicsResources(vtkWindow* win)
{
  this->Superclass::ReleaseGraphicsResources(win);
  this->Label->ReleaseGraphicsResources(win);
}

//----------------------------------------------------------------------
int vtkVolumeScaleRepresentation::RenderOverlay(vtkViewport* v)
{
  int count = 0;
  vtkRenderer* renderer = vtkRenderer::SafeDownCast(v);
  if (renderer) {
    this->SetRenderer(renderer);
    this->ScaleIfNecessary(v);
    this->Update2DLabel();
  }
  count += this->Superclass::RenderOverlay(v);
  this->Label->SetPropertyKeys(this->GetPropertyKeys());

  count += this->Label->RenderOverlay(v);
  return count;
}

//----------------------------------------------------------------------
int vtkVolumeScaleRepresentation::RenderOpaqueGeometry(vtkViewport* v)
{
  int count = 0;
  vtkRenderer* renderer = vtkRenderer::SafeDownCast(v);
  if (renderer) {
    this->SetRenderer(renderer);
    this->ScaleIfNecessary(v);
    this->Update2DLabel();
  }
  count += this->Superclass::RenderOpaqueGeometry(v);
  this->Label->SetPropertyKeys(this->GetPropertyKeys());
  count += this->Label->RenderOpaqueGeometry(v);
  return count;
}

//-----------------------------------------------------------------------------
int vtkVolumeScaleRepresentation::RenderTranslucentPolygonalGeometry(
  vtkViewport* v)
{
  int count = 0;
  vtkRenderer* renderer = vtkRenderer::SafeDownCast(v);
  if (renderer) {
    this->SetRenderer(renderer);
    this->ScaleIfNecessary(v);
    this->Update2DLabel();
  }
  count += this->Superclass::RenderTranslucentPolygonalGeometry(v);
  this->Label->SetPropertyKeys(this->GetPropertyKeys());
  count += this->Label->RenderTranslucentPolygonalGeometry(v);
  return count;
}

//-----------------------------------------------------------------------------
int vtkVolumeScaleRepresentation::HasTranslucentPolygonalGeometry()
{
  int result = 0;
  result |= this->Superclass::HasTranslucentPolygonalGeometry();
  result |= this->Label->HasTranslucentPolygonalGeometry();
  return result;
}

//----------------------------------------------------------------------
void vtkVolumeScaleRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  // Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);
}
