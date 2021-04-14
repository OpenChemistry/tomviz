#include "vtkTriangleBar.h"

#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkViewport.h>

vtkStandardNewMacro(vtkTriangleBar)

  vtkTriangleBar::vtkTriangleBar()
  : HorizontalPos(Position::End), VerticalPos(Position::Start), Edge(120)
{
  auto colors = vtkSmartPointer<vtkFloatArray>::New();
  colors->SetNumberOfComponents(3);
  colors->SetNumberOfTuples(3);

  this->Points->SetNumberOfPoints(3);
  this->Points->SetPoint(0, 10, 10, 0);
  this->Points->SetPoint(1, 160, 10, 0);
  this->Points->SetPoint(2, 160, 160, 0);
  this->Cells->InsertNextCell(4);
  this->Cells->InsertCellPoint(0);
  this->Cells->InsertCellPoint(1);
  this->Cells->InsertCellPoint(2);
  this->Cells->InsertCellPoint(0);
  this->Poly->GetPointData()->SetScalars(colors);
  this->Poly->SetPoints(this->Points);
  this->Poly->SetPolys(this->Cells);
  this->Mapper->SetInputData(this->Poly);
  this->Mapper->SetScalarModeToUsePointData();
  this->Mapper->SetColorModeToDirectScalars();
  this->BarActor->SetMapper(this->Mapper);

  double color0[3] = { 1, 0, 0 };
  double color1[3] = { 0, 1, 0 };
  double color2[3] = { 0, 0, 1 };
  this->SetColors(color0, color1, color2);

  this->LabelActor0->GetTextProperty()->SetJustificationToCentered();
  this->LabelActor1->GetTextProperty()->SetJustificationToCentered();
  this->LabelActor2->GetTextProperty()->SetJustificationToCentered();

  this->LabelActor0->GetTextProperty()->SetVerticalJustificationToTop();
  this->LabelActor1->GetTextProperty()->SetVerticalJustificationToTop();
  this->LabelActor2->GetTextProperty()->SetVerticalJustificationToBottom();

  this->SetLabels("Foo", "Bar", "Baz");
}

vtkTriangleBar::~vtkTriangleBar() {}

void vtkTriangleBar::SetColors(double color0[3], double color1[3],
                               double color2[3])
{
  auto colors = this->Poly->GetPointData()->GetScalars();

  colors->SetTuple(0, color0);
  colors->SetTuple(1, color1);
  colors->SetTuple(2, color2);
}

void vtkTriangleBar::SetLabels(const char* label0, const char* label1,
                               const char* label2)
{
  this->LabelActor0->SetInput(label0);
  this->LabelActor1->SetInput(label1);
  this->LabelActor2->SetInput(label2);
}

void vtkTriangleBar::SetAlignment(Position horizontalPos, Position verticalPos)
{
  this->HorizontalPos = horizontalPos;
  this->VerticalPos = verticalPos;
}

void vtkTriangleBar::updateRepresentation(vtkViewport* v)
{
  if (v->GetMTime() > this->updateTime) {
    int barSize[2] = { 0, 0 };
    barSize[0] = this->Edge;
    barSize[1] = sqrt(0.75 * this->Edge * this->Edge);
    const int margin = 15;
    const int labelMargin = 5;

    double labelSize0[3];
    double labelSize1[3];
    double labelSize2[3];
    this->LabelActor0->GetSize(v, labelSize0);
    this->LabelActor1->GetSize(v, labelSize1);
    this->LabelActor2->GetSize(v, labelSize2);

    auto displaySize = v->GetSize();
    int p0[2] = { 0, 0 };

    if (this->HorizontalPos == Position::Start) {
      p0[0] = margin + labelSize0[0] / 2;
    } else if (this->HorizontalPos == Position::Middle) {
      p0[0] = (displaySize[0] - barSize[0]) / 2;
    } else if (this->HorizontalPos == Position::End) {
      p0[0] = displaySize[0] - margin - labelSize1[0] * 0.5 - barSize[0];
    }

    if (this->VerticalPos == Position::Start) {
      p0[1] = margin + labelSize0[1] + labelMargin;
    } else if (this->VerticalPos == Position::Middle) {
      p0[1] = (displaySize[1] - barSize[1]) / 2;
    } else if (this->VerticalPos == Position::End) {
      p0[1] =
        displaySize[1] - margin - labelSize2[1] - labelMargin - barSize[1];
    }

    int p1[2] = { p0[0] + this->Edge, p0[1] };
    int p2[2] = { p0[0] + this->Edge / 2, p0[1] + barSize[1] };

    this->Points->SetPoint(0, p0[0], p0[1], 0);
    this->Points->SetPoint(1, p1[0], p1[1], 0);
    this->Points->SetPoint(2, p2[0], p2[1], 0);

    this->LabelActor0->SetPosition(p0[0], p0[1] - labelMargin);
    this->LabelActor1->SetPosition(p1[0], p1[1] - labelMargin);
    this->LabelActor2->SetPosition(p2[0], p2[1] + labelMargin);

    this->Points->Modified();

    this->updateTime.Modified();
  }
}

//----------------------------------------------------------------------
void vtkTriangleBar::GetActors(vtkPropCollection* pc)
{
  this->BarActor->GetActors(pc);
  this->LabelActor0->GetActors(pc);
  this->LabelActor1->GetActors(pc);
  this->LabelActor2->GetActors(pc);
}

void vtkTriangleBar::GetActors2D(vtkPropCollection* pc)
{
  this->BarActor->GetActors2D(pc);
  this->LabelActor0->GetActors2D(pc);
  this->LabelActor1->GetActors2D(pc);
  this->LabelActor2->GetActors2D(pc);
}

//----------------------------------------------------------------------------
void vtkTriangleBar::ReleaseGraphicsResources(vtkWindow* w)
{
  this->BarActor->ReleaseGraphicsResources(w);
  this->LabelActor0->ReleaseGraphicsResources(w);
  this->LabelActor1->ReleaseGraphicsResources(w);
  this->LabelActor2->ReleaseGraphicsResources(w);
}

//----------------------------------------------------------------------------
int vtkTriangleBar::RenderOpaqueGeometry(vtkViewport* v)
{
  int count = 0;
  // this->BuildRepresentation();
  count += this->BarActor->RenderOpaqueGeometry(v);
  count += this->LabelActor0->RenderOpaqueGeometry(v);
  count += this->LabelActor1->RenderOpaqueGeometry(v);
  count += this->LabelActor2->RenderOpaqueGeometry(v);

  return count;
}

//----------------------------------------------------------------------------
int vtkTriangleBar::RenderTranslucentPolygonalGeometry(vtkViewport* v)
{
  int count = 0;
  // this->UpdateRepresentation();
  count += this->BarActor->RenderTranslucentPolygonalGeometry(v);
  count += this->LabelActor0->RenderTranslucentPolygonalGeometry(v);
  count += this->LabelActor1->RenderTranslucentPolygonalGeometry(v);
  count += this->LabelActor2->RenderTranslucentPolygonalGeometry(v);

  return count;
}

//----------------------------------------------------------------------------
int vtkTriangleBar::RenderOverlay(vtkViewport* v)
{
  int count = 0;
  this->updateRepresentation(v);
  count += this->BarActor->RenderOverlay(v);
  count += this->LabelActor0->RenderOverlay(v);
  count += this->LabelActor1->RenderOverlay(v);
  count += this->LabelActor2->RenderOverlay(v);

  return count;
}

//----------------------------------------------------------------------------
vtkTypeBool vtkTriangleBar::HasTranslucentPolygonalGeometry()
{
  int result = 0;
  // this->BuildRepresentation();
  result |= this->BarActor->HasTranslucentPolygonalGeometry();
  result |= this->LabelActor0->HasTranslucentPolygonalGeometry();
  result |= this->LabelActor1->HasTranslucentPolygonalGeometry();
  result |= this->LabelActor2->HasTranslucentPolygonalGeometry();

  return result;
}
