/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkLengthScaleRepresentation.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkLengthScaleRepresentation.h"
#include "vtkAxisActor2D.h"
#include "vtkMath.h"
#include "vtkPointHandleRepresentation2D.h"
#include "vtkProperty2D.h"
#include "vtkRenderer.h"
#include "vtkObjectFactory.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkWindow.h"

#include <sstream>

vtkStandardNewMacro(vtkLengthScaleRepresentation);

//----------------------------------------------------------------------
vtkLengthScaleRepresentation::vtkLengthScaleRepresentation()
{
  this->Label = vtkTextActor::New();
  this->Label->SetVisibility(true);
  this->Label->GetTextProperty()->SetColor( 1.0, 1.0, 1.0 );
  this->Label->SetPosition(0,0);
  this->Label->GetTextProperty()->SetFontSize(40);
  this->Label->GetTextProperty()->SetJustificationToCentered();

  this->GetAxis()->SetRulerMode(1);
  this->GetAxis()->SetNumberOfMinorTicks(1);
  this->GetAxis()->SetTitleVisibility(0);

  this->InstantiateHandleRepresentation();
  this->MinRelativeScreenWidth = .03; // 3 % of the total viewier wdith
  this->MaxRelativeScreenWidth = .07; // 7 % of the total viewier width
  this->RescaleFactor = 2; // volume changes by 8 on update
  this->Length = 1.;
  this->UpdateRuler();

  this->LengthUnit = NULL;
  this->SetLengthUnit("unit");
  this->UpdateLabel();
}

//----------------------------------------------------------------------
vtkLengthScaleRepresentation::~vtkLengthScaleRepresentation()
{
  this->SetLengthUnit(NULL);
  this->Label->Delete();
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::BuildRepresentation()
{
  this->Superclass::BuildRepresentation();

  if (this->Label && (this->Label->GetMTime() > this->BuildTime ||
                      (this->Renderer && this->Renderer->GetVTKWindow() &&
                       this->Renderer->GetVTKWindow()->GetMTime() >
                       this->BuildTime)))
  {
    this->UpdateLabel();

    this->BuildTime.Modified();
  }
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::GetActors2D(vtkPropCollection *pc)
{
  this->Label->GetActors(pc);
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::UpdateRuler()
{
  double p1[3] = {-this->Length/2.,0.,0.};
  double p2[3] = {this->Length/2.,0.,0.};
  this->SetPoint1WorldPosition(p1);
  this->SetPoint2WorldPosition(p2);
  this->SetRulerDistance(this->Length/5.);
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::UpdateLabel()
{
  {
    std::stringstream s;
    s << this->Length << " " << std::string(this->LengthUnit);
    this->Label->SetInput(s.str().c_str());
  }

  double labelPosition[3] = {0.,0.,0.};
  if (this->Renderer)
  {
    this->Renderer->SetDisplayPoint(labelPosition);
    this->Renderer->WorldToDisplay();
    this->Renderer->GetDisplayPoint(labelPosition);

    double display1[3];
    this->Renderer->SetWorldPoint(this->AxisActor->GetPoint1());
    this->Renderer->WorldToDisplay();
    this->Renderer->GetDisplayPoint(display1);

    double display2[3];
    this->Renderer->SetWorldPoint(this->AxisActor->GetPoint2());
    this->Renderer->WorldToDisplay();
    this->Renderer->GetDisplayPoint(display2);
    labelPosition[0] = .5*(display2[0] + display1[0]);

    double bbox[4];
    this->Label->GetBoundingBox(this->Renderer, bbox);
    labelPosition[1] -= 1.25*(bbox[3] - bbox[2]);
  }

  this->Label->SetDisplayPosition(labelPosition[0], labelPosition[1]);
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::ScaleIfNecessary(vtkViewport* viewport)
{
  // Scaling is performed relative to the viewport window, so if there is no
  // window then there is nothing to do
  if (!viewport->GetVTKWindow())
  {
    return;
  }

  double display1[3];
  viewport->SetWorldPoint(this->AxisActor->GetPoint1());
  viewport->WorldToDisplay();
  viewport->GetDisplayPoint(display1);
  viewport->DisplayToNormalizedDisplay(display1[0],display1[1]);

  double display2[3];
  viewport->SetWorldPoint(this->AxisActor->GetPoint2());
  viewport->WorldToDisplay();
  viewport->GetDisplayPoint(display2);
  viewport->DisplayToNormalizedDisplay(display2[0],display2[1]);

  double relativeLength =
    sqrt(vtkMath::Distance2BetweenPoints(display1, display2));

  // We rescale our length using powers of our rescaling factor if it falls
  // outside of our bounds
  if (relativeLength > this->MaxRelativeScreenWidth)
  {
    int n = (log(relativeLength/this->MaxRelativeScreenWidth) /
             log(this->RescaleFactor));
    this->SetLength(this->Length / pow(this->RescaleFactor,n));
    this->Modified();
  }
  else if (relativeLength < this->MinRelativeScreenWidth)
  {
    int n = ceil(log(this->MinRelativeScreenWidth/relativeLength) /
                 log(this->RescaleFactor));
    this->SetLength(this->Length * pow(this->RescaleFactor,n));
    this->Modified();
  }
}

//------------------------------------------------------------------------------
void vtkLengthScaleRepresentation::SetLength(double d)
{
  if (this->Length != (d > 0. ? d : 0.))
  {
    this->Length = d;
    this->UpdateRuler();
    this->UpdateLabel();
    this->Modified();
  }
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::SetRepresentationVisibility(bool choice)
{
  this->Superclass::SetVisibility(choice);
  this->Label->SetVisibility(choice);
  this->Modified();
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::ReleaseGraphicsResources(
  vtkWindow *win)
{
  this->Superclass::ReleaseGraphicsResources(win);
  this->Label->ReleaseGraphicsResources(win);
}

//----------------------------------------------------------------------
int vtkLengthScaleRepresentation::RenderOverlay(vtkViewport *v)
{
  int count=0;
  vtkRenderer* renderer = vtkRenderer::SafeDownCast(v);
  if (renderer)
  {
    this->SetRenderer(renderer);
    this->ScaleIfNecessary(v);
  }
  count += this->Superclass::RenderOverlay(v);
  this->Label->SetPropertyKeys(this->GetPropertyKeys());

  count += this->Label->RenderOverlay(v);
  return count;
}

//----------------------------------------------------------------------
int vtkLengthScaleRepresentation::RenderOpaqueGeometry(vtkViewport *v)
{
  int count=0;
  vtkRenderer* renderer = vtkRenderer::SafeDownCast(v);
  if (renderer)
  {
    this->SetRenderer(renderer);
  }
  count += this->Superclass::RenderOpaqueGeometry(v);
  this->Label->SetPropertyKeys(this->GetPropertyKeys());
  count += this->Label->RenderOpaqueGeometry(v);
  return count;
}

//-----------------------------------------------------------------------------
int vtkLengthScaleRepresentation::RenderTranslucentPolygonalGeometry(
  vtkViewport *v)
{
  int count=0;
  vtkRenderer* renderer = vtkRenderer::SafeDownCast(v);
  if (renderer)
  {
    this->SetRenderer(renderer);
  }
  count += this->Superclass::RenderTranslucentPolygonalGeometry(v);
  this->Label->SetPropertyKeys(this->GetPropertyKeys());
  count += this->Label->RenderTranslucentPolygonalGeometry(v);
  return count;
}

//-----------------------------------------------------------------------------
int vtkLengthScaleRepresentation::HasTranslucentPolygonalGeometry()
{
  int result=0;
  result |= this->Superclass::HasTranslucentPolygonalGeometry();
  result |= this->Label->HasTranslucentPolygonalGeometry();
  return result;
}

//------------------------------------------------------------------------------
namespace
{
  const double RelativeScreenWidthUpperLimit = 1.;
  const double RelativeScreenWidthLowerLimit = 1.e-2;
}

//------------------------------------------------------------------------------
void vtkLengthScaleRepresentation::SetMinRelativeScreenWidth(double d)
{
  if (this->MinRelativeScreenWidth !=
      (d < RelativeScreenWidthLowerLimit ?
       RelativeScreenWidthLowerLimit :
       (d > RelativeScreenWidthUpperLimit ?
        RelativeScreenWidthUpperLimit : d)))
  {
    this->MinRelativeScreenWidth = d;
    if (this->MaxRelativeScreenWidth <
        this->RescaleFactor*this->MinRelativeScreenWidth)
    {
      this->MaxRelativeScreenWidth =
        1.1*this->RescaleFactor*this->MinRelativeScreenWidth;
      if (this->MaxRelativeScreenWidth > RelativeScreenWidthUpperLimit)
      {
        this->MaxRelativeScreenWidth = RelativeScreenWidthUpperLimit;
        this->MinRelativeScreenWidth =
          .9*this->RescaleFactor*this->MaxRelativeScreenWidth;
      }
    }
    this->Modified();
  }
}

//------------------------------------------------------------------------------
void vtkLengthScaleRepresentation::SetMaxRelativeScreenWidth(double d)
{
  if (this->MaxRelativeScreenWidth !=
      (d < RelativeScreenWidthLowerLimit ?
       RelativeScreenWidthLowerLimit :
       (d > RelativeScreenWidthUpperLimit ?
        RelativeScreenWidthUpperLimit : d)))
  {
    this->MaxRelativeScreenWidth = d;
    if (this->MaxRelativeScreenWidth <
        this->RescaleFactor*this->MinRelativeScreenWidth)
    {
      this->MinRelativeScreenWidth =
        .9*this->RescaleFactor*this->MaxRelativeScreenWidth;
      if (this->MinRelativeScreenWidth < RelativeScreenWidthLowerLimit)
      {
        this->MinRelativeScreenWidth = RelativeScreenWidthLowerLimit;
        this->MaxRelativeScreenWidth =
          1.1*this->RescaleFactor*this->MinRelativeScreenWidth;
      }
    }
    this->Modified();
  }
}

//----------------------------------------------------------------------
void vtkLengthScaleRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os,indent);
}
