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
#include "vtkCallbackCommand.h"
#include "vtkChartTransfer2DEditor.h"
#include "vtkColorTransferFunction.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPlotHistogram2D.h"
#include "vtkPNGWriter.h"
#include "vtkPointData.h"
#include "vtkRect.h"
#include "vtkTransferFunctionBoxItem.h"
#include "vtkUnsignedCharArray.h"


class vtkChartTransfer2DEditor::Private
{
public:
  Private()
  {
  };

  vtkImageData* Transfer2D = nullptr;
  vtkNew<vtkCallbackCommand> Callback;
};

////////////////////////////////////////////////////////////////////////////////
vtkStandardNewMacro(vtkChartTransfer2DEditor);

//-----------------------------------------------------------------------------
vtkChartTransfer2DEditor::vtkChartTransfer2DEditor()
: Storage(new Private)
{
  this->Storage->Callback->SetClientData(this);
  this->Storage->Callback->SetCallback(
    vtkChartTransfer2DEditor::OnBoxItemModified);
}

//-----------------------------------------------------------------------------
vtkChartTransfer2DEditor::~vtkChartTransfer2DEditor()
{
  delete Storage;
}

//-----------------------------------------------------------------------------
void vtkChartTransfer2DEditor::SetTransfer2D(vtkImageData* transfer2D)
{
  if (transfer2D != this->Storage->Transfer2D)
  {
    if (this->Storage->Transfer2D != nullptr)
    {
      this->Storage->Transfer2D->UnRegister(this);
    }

    this->Storage->Transfer2D = transfer2D;
    if (this->Storage->Transfer2D != nullptr)
    {
      this->Storage->Transfer2D->Register(this);
    }

    this->Modified();
    this->GenerateTransfer2D();
  }
}

//-----------------------------------------------------------------------------
void vtkChartTransfer2DEditor::GenerateTransfer2D()
{
  if (!this->Storage->Transfer2D)
  {
    vtkErrorMacro(<< "Failed to generate Transfer2D!");
    return;
  }

  // Update size (match the number of bins of the histogram)
  int bins[3];
  this->Histogram->GetInputImageData()->GetDimensions(bins);
  this->Storage->Transfer2D->SetDimensions(bins[0], bins[1], 1);
  this->Storage->Transfer2D->AllocateScalars(VTK_UNSIGNED_CHAR, 4);

  // Initialize as fully transparent
  vtkUnsignedCharArray* arr = vtkUnsignedCharArray::SafeDownCast(
    this->Storage->Transfer2D->GetPointData()->GetScalars());
  void* dataPtr = arr->GetVoidPointer(0);
  memset(dataPtr, 0, bins[0] * bins[1] * 4 * sizeof(unsigned char));

  // Raster each box into the 2D table
  const vtkIdType numPlots = this->GetNumberOfPlots();
  for (vtkIdType i = 0; i < numPlots; i++)
  {
    typedef vtkTransferFunctionBoxItem BoxType;
    BoxType* boxItem = BoxType::SafeDownCast(this->GetPlot(i));
    if (!boxItem)
    {
      continue;
    }

    this->RasterBoxItem(boxItem);
  }

  this->InvokeEvent(vtkCommand::EndEvent);
}

//-----------------------------------------------------------------------------
vtkPlot* vtkChartTransfer2DEditor::GetPlot(vtkIdType index)
{
  return vtkChartXY::GetPlot(index);
}

//-----------------------------------------------------------------------------
void vtkChartTransfer2DEditor::RasterBoxItem(vtkTransferFunctionBoxItem* boxItem)
{
  const vtkRectd& box = boxItem->GetBox();
  vtkPiecewiseFunction* opacFunc = boxItem->GetOpacityFunction();
  vtkColorTransferFunction* colorFunc = boxItem->GetColorFunction();
  if (!opacFunc || !colorFunc)
  {
    vtkErrorMacro(<< "BoxItem contains invalid transfer functions!");
    return;
  }

  //GetTransferFunction(to the actual ranges and number of bins)
  const vtkIdType width = static_cast<vtkIdType>(box.GetWidth());
  const vtkIdType height = static_cast<vtkIdType>(box.GetHeight());

  /// TODO This assumes color and opacity share the same data range
  double range[2];
  colorFunc->GetRange(range);

  double* dataRGB = new double[width * 3];
  colorFunc->GetTable(range[0], range[1], width, dataRGB);

  double* dataAlpha = new double[width];
  opacFunc->GetTable(range[0], range[1], width, dataAlpha);

  //Copy the values into this->Transfer2D
  vtkUnsignedCharArray* transfer = vtkUnsignedCharArray::SafeDownCast(
    this->Storage->Transfer2D->GetPointData()->GetScalars());

  const vtkIdType x0 = static_cast<vtkIdType>(box.GetX());
  const vtkIdType y0 = static_cast<vtkIdType>(box.GetY());

  int bins[3];
  this->Storage->Transfer2D->GetDimensions(bins);

  for (vtkIdType j = 0; j < height; j++)
    for (vtkIdType i = 0; i < width; i++)
    {
      double color[4];

      color[0] = dataRGB[i * 3] * 255.0;
      color[1] = dataRGB[i * 3 + 1] * 255.0;
      color[2] = dataRGB[i * 3 + 2] * 255.0;
      color[3] = dataAlpha[i] * 255.0;

      const vtkIdType index = (y0 + j) * bins[1] + (x0 + i);
      transfer->SetTuple(index, color);
    }

  vtkPNGWriter* pngWriter = vtkPNGWriter::New();
  pngWriter->SetInputData(this->Storage->Transfer2D);
  pngWriter->SetFileName("/tmp/transfer2d.png");
  pngWriter->Update();
  pngWriter->Write();
  pngWriter->Delete();

  // Cleanup
  delete dataRGB;
  delete dataAlpha;
}

//-----------------------------------------------------------------------------
vtkIdType vtkChartTransfer2DEditor::AddPlot(vtkPlot* plot)
{
  plot->AddObserver(vtkCommand::EndEvent, this->Storage->Callback.GetPointer());
  return Superclass::AddPlot(plot);
}

//-----------------------------------------------------------------------------
void vtkChartTransfer2DEditor::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//-----------------------------------------------------------------------------
void vtkChartTransfer2DEditor::OnBoxItemModified(vtkObject* caller,
  unsigned long eid, void *clientData, void* callData)
{
  vtkChartTransfer2DEditor* self =
    reinterpret_cast<vtkChartTransfer2DEditor*>(clientData);
  self->GenerateTransfer2D();
}
