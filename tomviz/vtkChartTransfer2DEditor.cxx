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
#include <vtkTransferFunctionBoxItem.h>

vtkStandardNewMacro(vtkChartTransfer2DEditor)

vtkChartTransfer2DEditor::vtkChartTransfer2DEditor()
{
  Callback->SetClientData(this);
  Callback->SetCallback(vtkChartTransfer2DEditor::OnBoxItemModified);
  /// TODO Disable panning
}

vtkChartTransfer2DEditor::~vtkChartTransfer2DEditor() = default;

void vtkChartTransfer2DEditor::SetTransfer2D(vtkImageData* transfer2D)
{
  if (transfer2D != Transfer2D) {
    if (Transfer2D != nullptr) {
      Transfer2D->UnRegister(this);
    }

    Transfer2D = transfer2D;
    if (Transfer2D != nullptr) {
      Transfer2D->Register(this);
    }

    Modified();
    GenerateTransfer2D();
  }
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

    RasterBoxItem(boxItem);
  }

  InvokeEvent(vtkCommand::EndEvent);
}

vtkPlot* vtkChartTransfer2DEditor::GetPlot(vtkIdType index)
{
  return vtkChartXY::GetPlot(index);
}

void vtkChartTransfer2DEditor::RasterBoxItem(
  vtkTransferFunctionBoxItem* boxItem)
{
  const vtkRectd& box = boxItem->GetBox();
  vtkPiecewiseFunction* opacFunc = boxItem->GetOpacityFunction();
  vtkColorTransferFunction* colorFunc = boxItem->GetColorFunction();
  if (!opacFunc || !colorFunc) {
    vtkErrorMacro(<< "BoxItem contains invalid transfer functions!");
    return;
  }

  double spacing[3];
  Histogram->GetInputImageData()->GetSpacing(spacing);
  const vtkIdType width = static_cast<vtkIdType>(box.GetWidth() / spacing[0]);
  const vtkIdType height = static_cast<vtkIdType>(box.GetHeight() / spacing[1]);

  if (width <= 0 || height <= 0) {
    return;
  }

  // Assume color and opacity share the same data range
  double range[2];
  colorFunc->GetRange(range);

  double* dataRGB = new double[width * 3];
  colorFunc->GetTable(range[0], range[1], width, dataRGB);

  double* dataAlpha = new double[width];
  opacFunc->GetTable(range[0], range[1], width, dataAlpha);

  // Copy the values into Transfer2D
  vtkFloatArray* transfer =
    vtkFloatArray::SafeDownCast(Transfer2D->GetPointData()->GetScalars());

  const vtkIdType x0 = static_cast<vtkIdType>(box.GetX() / spacing[0]);
  const vtkIdType y0 = static_cast<vtkIdType>(box.GetY() / spacing[1]);

  int bins[3];
  Transfer2D->GetDimensions(bins);

  for (vtkIdType j = 0; j < height; j++)
    for (vtkIdType i = 0; i < width; i++) {
      double color[4];

      color[0] = dataRGB[i * 3];
      color[1] = dataRGB[i * 3 + 1];
      color[2] = dataRGB[i * 3 + 2];
      color[3] = dataAlpha[i];

      const vtkIdType index = (y0 + j) * bins[1] + (x0 + i);
      transfer->SetTuple(index, color);
    }

  delete[] dataRGB;
  delete[] dataAlpha;
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
  const double deltaX = (xRange[1] - xRange[0]) / 3.0;
  const double deltaY = (yRange[1] - yRange[0]) / 3.0;
  item->SetBox(xRange[0] + deltaX, yRange[0] + deltaY, deltaX, deltaY);
}
