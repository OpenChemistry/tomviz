/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkChartTransfer2DEditor.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkChartTransfer2DEditor.h"

#include <vtkAxis.h>
#include <vtkCallbackCommand.h>
#include <vtkColorTransferFunction.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPNGWriter.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPlotHistogram2D.h>
#include <vtkPointData.h>
#include <vtkRect.h>
#include <vtkTextProperty.h>
#include <vtkTransferFunctionBoxItem.h>
#include <vtkTooltipItem.h>

vtkStandardNewMacro(vtkChartTransfer2DEditor)

  vtkChartTransfer2DEditor::vtkChartTransfer2DEditor()
  : Transfer2DBox(&this->DummyBox)
{
  Callback->SetClientData(this);
  Callback->SetCallback(vtkChartTransfer2DEditor::OnBoxItemModified);

  int fontSize = 8;
  GetAxis(vtkAxis::LEFT)->GetLabelProperties()->SetFontSize(fontSize);
  GetAxis(vtkAxis::BOTTOM)->GetLabelProperties()->SetFontSize(fontSize);
  GetAxis(vtkAxis::RIGHT)->GetLabelProperties()->SetFontSize(fontSize);
  GetAxis(vtkAxis::LEFT)->GetTitleProperties()->SetFontSize(fontSize);
  GetAxis(vtkAxis::BOTTOM)->GetTitleProperties()->SetFontSize(fontSize);
  GetTooltip()->GetTextProperties()->SetFontSize(fontSize);
}

vtkChartTransfer2DEditor::~vtkChartTransfer2DEditor() = default;

void vtkChartTransfer2DEditor::SetTransfer2D(vtkImageData* transfer2D,
                                             vtkRectd* box)
{
  // Set the box first, GenerateTransfer2D writes to it
  // so avoid writing to the old one.
  if (box != nullptr) {
    this->Transfer2DBox = box;
  } else {
    // Make sure to use internal dummy box.  This is set to
    // nullptr when the active datasource/module is cleared
    // which usually means it was deleted.  Avoid pointers to
    // deleted data by setting this to a local dummy value.
    this->Transfer2DBox = &this->DummyBox;
  }
  if (transfer2D != this->Transfer2D) {
    this->Transfer2D = transfer2D;

    // Call modified here but delay call of GenerateTransfer2D until
    // after the box update is passed to the boxItem.
    Modified();
  }
  // Now force the new box through (must be done after the
  // Transfer2D is set).
  if (box != nullptr) {
    // Update the box shown on the plot (assumes only one box).
    const vtkIdType numPlots = GetNumberOfPlots();
    for (vtkIdType i = 0; i < numPlots; i++) {
      typedef vtkTransferFunctionBoxItem BoxType;
      BoxType* boxItem = BoxType::SafeDownCast(GetPlot(i));
      if (!boxItem) {
        continue;
      }

      boxItem->SetBox(box->GetX(), box->GetY(), box->GetWidth(),
                      box->GetHeight());
      break;
    }
  }

  // This should only do something if Modified was called and it has not
  // been called since.
  GenerateTransfer2D();
}

bool vtkChartTransfer2DEditor::IsInitialized()
{
  return Transfer2D && Histogram->GetInputImageData();
}

void vtkChartTransfer2DEditor::GenerateTransfer2D()
{
  if (!IsInitialized()) {
    return;
  }

  // Update size (match the number of bins of the histogram)
  int bins[3];
  Histogram->GetInputImageData()->GetDimensions(bins);
  Transfer2D->SetDimensions(bins[0], bins[1], 1);
  Transfer2D->AllocateScalars(VTK_FLOAT, 4);

  // Initialize as fully transparent
  vtkFloatArray* arr =
    vtkFloatArray::SafeDownCast(Transfer2D->GetPointData()->GetScalars());
  void* dataPtr = arr->GetVoidPointer(0);
  memset(dataPtr, 0, bins[0] * bins[1] * 4 * sizeof(float));

  // Raster each box into the 2D table
  const vtkIdType numPlots = GetNumberOfPlots();
  for (vtkIdType i = 0; i < numPlots; i++) {
    typedef vtkTransferFunctionBoxItem BoxType;
    BoxType* boxItem = BoxType::SafeDownCast(GetPlot(i));
    if (!boxItem) {
      continue;
    }

    *this->Transfer2DBox = boxItem->GetBox();
    vtkTransferFunctionBoxItem::rasterTransferFunction2DBox(
      Histogram->GetInputImageData(), boxItem->GetBox(), Transfer2D,
      boxItem->GetColorFunction(), boxItem->GetOpacityFunction());
  }

  InvokeEvent(vtkCommand::EndEvent);
}

vtkPlot* vtkChartTransfer2DEditor::GetPlot(vtkIdType index)
{
  return vtkChartXY::GetPlot(index);
}

vtkIdType vtkChartTransfer2DEditor::AddFunction(
  vtkTransferFunctionBoxItem* boxItem)
{
  if (!IsInitialized()) {
    return -1;
  }

  double xRange[2];
  auto bottomAxis = GetAxis(vtkAxis::BOTTOM);
  bottomAxis->GetRange(xRange);

  double yRange[2];
  auto leftAxis = GetAxis(vtkAxis::LEFT);
  leftAxis->GetRange(yRange);

  // Set bounds in the box item so that it can only move within the
  // histogram's range.
  boxItem->SetValidBounds(xRange[0], xRange[1], yRange[0], yRange[1]);
  SetDefaultBoxPosition(boxItem, xRange, yRange);
  boxItem->AddObserver(vtkCommand::SelectionChangedEvent,
                       Callback.GetPointer());

  return AddPlot(boxItem);
}

vtkIdType vtkChartTransfer2DEditor::AddPlot(vtkPlot* plot)
{
  return Superclass::AddPlot(plot);
}

void vtkChartTransfer2DEditor::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

/// TODO Use linear interpolation for the histogram texture
// vtkChartTransfer2DEditor::Paint

void vtkChartTransfer2DEditor::OnBoxItemModified(vtkObject* vtkNotUsed(caller),
                                                 unsigned long vtkNotUsed(eid),
                                                 void* clientData,
                                                 void* vtkNotUsed(callData))
{
  vtkChartTransfer2DEditor* self =
    reinterpret_cast<vtkChartTransfer2DEditor*>(clientData);
  self->GenerateTransfer2D();
}

void vtkChartTransfer2DEditor::SetInputData(vtkImageData* data, vtkIdType z)
{
  int bins[3];
  double origin[3], spacing[3];
  data->GetOrigin(origin);
  data->GetDimensions(bins);
  data->GetSpacing(spacing);

  // Compute image bounds
  const double xMin = origin[0];
  const double xMax = bins[0] * spacing[0];
  const double yMin = origin[1];
  const double yMax = bins[1] * spacing[1];

  auto axis = GetAxis(vtkAxis::BOTTOM);
  axis->SetRange(xMin, xMax);
  axis = GetAxis(vtkAxis::LEFT);
  axis->SetRange(yMin, yMax);

  UpdateItemsBounds(xMin, xMax, yMin, yMax);
  vtkChartHistogram2D::SetInputData(data, z);
}

void vtkChartTransfer2DEditor::UpdateItemsBounds(const double xMin,
                                                 const double xMax,
                                                 const double yMin,
                                                 const double yMax)
{
  // Set the new bounds to its current box items (plots).
  const vtkIdType numPlots = GetNumberOfPlots();
  for (vtkIdType i = 0; i < numPlots; i++) {
    auto boxItem = vtkControlPointsItem::SafeDownCast(GetPlot(i));
    if (!boxItem) {
      continue;
    }

    boxItem->SetValidBounds(xMin, xMax, yMin, yMax);
  }
}

void vtkChartTransfer2DEditor::SetDefaultBoxPosition(
  vtkSmartPointer<vtkTransferFunctionBoxItem> item, const double xRange[2],
  const double yRange[2])
{
  if (this->Transfer2DBox->GetWidth() < 0) {
    const double deltaX = (xRange[1] - xRange[0]) / 3.0;
    const double deltaY = (yRange[1] - yRange[0]) / 3.0;
    item->SetBox(xRange[0] + deltaX, yRange[0] + deltaY, deltaX, deltaY);
    // set the box in the source directly since the callback may not be set up
    *this->Transfer2DBox = item->GetBox();
  }
}
