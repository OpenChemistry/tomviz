/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "vtkChartHistogram.h"

#include "vtkAxis.h"
#include "vtkCommand.h"
#include "vtkContext2D.h"
#include "vtkContextMouseEvent.h"
#include "vtkContextScene.h"
#include "vtkObjectFactory.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPiecewiseFunctionItem.h"
#include "vtkPiecewiseControlPointsItem.h"
#include "vtkPen.h"
#include "vtkPlot.h"
#include "vtkPlotBar.h"
#include "vtkScalarsToColors.h"
#include "vtkTable.h"
#include "vtkTransform2D.h"

//-----------------------------------------------------------------------------
class vtkHistogramMarker : public vtkPlot
{
public:
  static vtkHistogramMarker * New();
  double PositionX;

  bool Paint(vtkContext2D *painter) override
  {
    vtkNew<vtkPen> pen;
    pen->SetColor(255, 0, 0, 255);
    pen->SetWidth(2.0);
    painter->ApplyPen(pen.Get());
    painter->DrawLine(PositionX, 0, PositionX, 1e9);
    return true;
  }
};
vtkStandardNewMacro(vtkHistogramMarker)

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkChartHistogram)

//-----------------------------------------------------------------------------
vtkChartHistogram::vtkChartHistogram()
{
  this->SetBarWidthFraction(1.0);
  this->SetRenderEmpty(true);
  this->SetAutoAxes(false);
  this->ZoomWithMouseWheelOff();
  this->GetAxis(vtkAxis::LEFT)->SetTitle("");
  this->GetAxis(vtkAxis::BOTTOM)->SetTitle("");
  this->GetAxis(vtkAxis::BOTTOM)->SetBehavior(vtkAxis::FIXED);
  this->GetAxis(vtkAxis::BOTTOM)->SetRange(0, 255);
  this->GetAxis(vtkAxis::LEFT)->SetBehavior(vtkAxis::FIXED);
  this->GetAxis(vtkAxis::LEFT)->SetRange(0.0001, 10);
  this->GetAxis(vtkAxis::LEFT)->SetMinimumLimit(1);
  this->GetAxis(vtkAxis::LEFT)->SetLogScale(true);
  this->GetAxis(vtkAxis::RIGHT)->SetBehavior(vtkAxis::FIXED);
  this->GetAxis(vtkAxis::RIGHT)->SetRange(0.0, 1.0);
  this->GetAxis(vtkAxis::RIGHT)->SetVisible(true);

  // Set up the plot bar
  this->AddPlot(this->HistogramPlotBar.Get());
  this->HistogramPlotBar->SetColor(0, 0, 255, 255);
  this->HistogramPlotBar->GetPen()->SetLineType(vtkPen::NO_PEN);

  // Set up and add the opacity editor chart items
  this->OpacityFunctionItem->SetOpacity(0.0); // don't show the transfer function
  this->AddPlot(this->OpacityFunctionItem.Get());
  this->SetPlotCorner(this->OpacityFunctionItem.Get(), 1);

  this->OpacityControlPointsItem->SetEndPointsXMovable(false);
  this->OpacityControlPointsItem->SetEndPointsYMovable(true);
  this->OpacityControlPointsItem->SetEndPointsRemovable(false);

  vtkPen* pen = this->OpacityControlPointsItem->GetPen();
  pen->SetLineType(vtkPen::SOLID_LINE);
  pen->SetColor(0, 0, 0);
  pen->SetOpacity(255);
  pen->SetWidth(2.0);
  this->AddPlot(this->OpacityControlPointsItem.Get());
  this->SetPlotCorner(this->OpacityControlPointsItem.Get(), 1);
}

//-----------------------------------------------------------------------------
bool vtkChartHistogram::MouseDoubleClickEvent(const vtkContextMouseEvent &m)
{
  // Determine the location of the click, and emit something we can listen to!
  vtkPlotBar *histo = nullptr;
  if (this->GetNumberOfPlots() > 0)
  {
    histo = vtkPlotBar::SafeDownCast(this->GetPlot(0));
  }
  if (!histo)
  {
    return false;
  }
  this->CalculateUnscaledPlotTransform(histo->GetXAxis(), histo->GetYAxis(),
                                       this->Transform.Get());
  vtkVector2f pos;
  this->Transform->InverseTransformPoints(m.GetScenePos().GetData(), pos.GetData(),
                                          1);
  this->PositionX = pos.GetX();
  this->Marker->PositionX = this->PositionX;
  this->Marker->Modified();
  this->Scene->SetDirty(true);
  if (this->GetNumberOfPlots() == 1)
  {
    // Work around a bug in the charts - ensure corner is invalid for the plot.
    this->Marker->SetXAxis(nullptr);
    this->Marker->SetYAxis(nullptr);
    this->AddPlot(this->Marker.Get());
  }
  this->InvokeEvent(vtkCommand::CursorChangedEvent);
  return true;
}

//-----------------------------------------------------------------------------
void vtkChartHistogram::SetHistogramInputData(vtkTable* table,
                                              const char* xAxisColumn,
                                              const char* yAxisColumn)
{
  this->HistogramPlotBar->SetInputData(table, xAxisColumn, yAxisColumn);
}

//-----------------------------------------------------------------------------
void vtkChartHistogram::SetScalarVisibility(bool visible)
{
  this->HistogramPlotBar->SetScalarVisibility(visible);
}

//-----------------------------------------------------------------------------
void vtkChartHistogram::ScalarVisibilityOn()
{
  this->HistogramPlotBar->ScalarVisibilityOn();
}

//-----------------------------------------------------------------------------
void vtkChartHistogram::SetLookupTable(vtkScalarsToColors* lut)
{
  this->HistogramPlotBar->SetLookupTable(lut);
}

//-----------------------------------------------------------------------------
void vtkChartHistogram::SelectColorArray(const char* arrayName)
{
  this->HistogramPlotBar->SelectColorArray(arrayName);
}

//-----------------------------------------------------------------------------
void vtkChartHistogram::SetOpacityFunction(vtkPiecewiseFunction* opacityFunction)
{
  this->OpacityFunctionItem->SetPiecewiseFunction(opacityFunction);
  this->OpacityControlPointsItem->SetPiecewiseFunction(opacityFunction);
}
