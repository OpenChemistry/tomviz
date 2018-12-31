/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "vtkChartGradientOpacityEditor.h"

#include <vtkAxis.h>
#include <vtkChart.h>
#include <vtkColorTransferControlPointsItem.h>
#include <vtkColorTransferFunction.h>
#include <vtkColorTransferFunctionItem.h>
#include <vtkContextItem.h>
#include <vtkContextScene.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkScalarsToColors.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>
#include <vtkTextProperty.h>
#include <vtkVector.h>

#include "vtkChartHistogram.h"

class vtkChartGradientOpacityEditor::PIMPL
{
public:
  PIMPL() : Geometry(0, 0), NeedsUpdate(true) {}
  ~PIMPL() {}

  void ForwardEvent(vtkObject* vtkNotUsed(object), unsigned long eventId,
                    void* vtkNotUsed(data))
  {
    this->Self->InvokeEvent(eventId);
  }

  // Cached geometry of the chart
  vtkVector2i Geometry;

  // Dirty bit
  bool NeedsUpdate;

  // Reference to owner of the PIMPL
  vtkChartGradientOpacityEditor* Self;
};

vtkStandardNewMacro(vtkChartGradientOpacityEditor)

  vtkChartGradientOpacityEditor::vtkChartGradientOpacityEditor()
{
  this->Private = new PIMPL();
  this->Private->Self = this;

  this->Borders[vtkAxis::LEFT] = 8;
  this->Borders[vtkAxis::BOTTOM] = 8;
  this->Borders[vtkAxis::RIGHT] = 8;
  this->Borders[vtkAxis::TOP] = 20;

  this->HistogramChart->SetHiddenAxisBorder(10);
  this->HistogramChart->SetLayoutStrategy(vtkChart::AXES_TO_RECT);
  this->AddItem(this->HistogramChart.Get());

  vtkAxis* bottomAxis = this->HistogramChart->GetAxis(vtkAxis::BOTTOM);
  bottomAxis->SetTitle("Gradient Magnitude");
  bottomAxis->GetTitleProperties()->SetFontSize(8);

  // Forward events from internal charts to observers of this object
  this->HistogramChart->AddObserver(vtkCommand::CursorChangedEvent,
                                    this->Private, &PIMPL::ForwardEvent);
}

vtkChartGradientOpacityEditor::~vtkChartGradientOpacityEditor()
{
  delete this->Private;
}

void vtkChartGradientOpacityEditor::SetHistogramInputData(
  vtkTable* table, const char* xAxisColumn, const char* yAxisColumn)
{
  this->HistogramChart->SetHistogramInputData(table, xAxisColumn, yAxisColumn);
  this->HistogramChart->SetHistogramVisible(false);
  this->HistogramChart->SetMarkerVisible(false);
  this->HistogramChart->GetAxis(vtkAxis::LEFT)->SetRange(0.0, 1.0);
  this->HistogramChart->GetAxis(vtkAxis::LEFT)->SetLogScale(false);

  // The data range may change and cause the labels to change. Hence, update
  // the geometry.
  this->Private->NeedsUpdate = true;
}

void vtkChartGradientOpacityEditor::SetScalarVisibility(bool visible)
{
  this->HistogramChart->SetScalarVisibility(visible);
}

void vtkChartGradientOpacityEditor::SelectColorArray(const char* arrayName)
{
  this->HistogramChart->SelectColorArray(arrayName);
}

void vtkChartGradientOpacityEditor::SetOpacityFunction(
  vtkPiecewiseFunction* opacityFunction)
{
  this->HistogramChart->SetOpacityFunction(opacityFunction);
}

vtkAxis* vtkChartGradientOpacityEditor::GetHistogramAxis(int axis)
{
  return this->HistogramChart->GetAxis(axis);
}

void vtkChartGradientOpacityEditor::SetDPI(int dpi)
{
  if (this->HistogramChart.Get()) {
    this->HistogramChart->SetDPI(dpi);
  }
}

bool vtkChartGradientOpacityEditor::Paint(vtkContext2D* painter)
{
  vtkContextScene* scene = this->GetScene();
  int sceneWidth = scene->GetSceneWidth();
  int sceneHeight = scene->GetSceneHeight();
  if (this->Private->NeedsUpdate ||
      sceneWidth != this->Private->Geometry.GetX() ||
      sceneHeight != this->Private->Geometry.GetY()) {
    this->Private->NeedsUpdate = false;

    // Update the geometry size cache
    this->Private->Geometry.Set(sceneWidth, sceneHeight);

    // Upper chart (histogram) expands, lower chart (color bar) is fixed height.
    float x = this->Borders[vtkAxis::LEFT];
    float y = this->Borders[vtkAxis::BOTTOM];

    // Add the width of the left axis to x to make room for y labels
    this->GetHistogramAxis(vtkAxis::LEFT)->Update();
    float leftAxisWidth = this->GetHistogramAxis(vtkAxis::LEFT)
                            ->GetBoundingRect(painter)
                            .GetWidth();
    x += leftAxisWidth;
    float plotWidth = sceneWidth - x - this->Borders[vtkAxis::RIGHT];

    this->GetHistogramAxis(vtkAxis::BOTTOM)->Update();
    float bottomAxisHeight = this->GetHistogramAxis(vtkAxis::BOTTOM)
                               ->GetBoundingRect(painter)
                               .GetHeight();
    float verticalMargin = bottomAxisHeight;
    y += verticalMargin - 5;
    vtkRectf histogramChart(x, y, plotWidth,
                            sceneHeight - y - this->Borders[vtkAxis::TOP]);
    this->HistogramChart->SetSize(histogramChart);
    this->HistogramChart->GetAxis(vtkAxis::LEFT)->Modified();
  }

  return this->Superclass::Paint(painter);
}
