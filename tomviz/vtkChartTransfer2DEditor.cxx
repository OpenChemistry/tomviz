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
#include "vtkColorTransferFunction.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPlotHistogram2D.h"
#include "vtkPointData.h"
#include "vtkRect.h"
#include "vtkTransferFunctionBoxItem.h"
#include "vtkUnsignedCharArray.h"


class vtkChartTransfer2DEditor::Private
{
public:
  Private()
  {
    this->Transfer2D->SetDimensions(64, 64, 1);
  }

  vtkNew<vtkImageData> Transfer2D;
};

////////////////////////////////////////////////////////////////////////////////
vtkStandardNewMacro(vtkChartTransfer2DEditor);

//-----------------------------------------------------------------------------
vtkChartTransfer2DEditor::vtkChartTransfer2DEditor()
: Storage(new Private)
{
}

//-----------------------------------------------------------------------------
vtkChartTransfer2DEditor::~vtkChartTransfer2DEditor()
{
  delete Storage;
}

//-----------------------------------------------------------------------------
vtkImageData* vtkChartTransfer2DEditor::GetTransfer2D()
{
  //if (this->GetMTime() > this->Storage->Transfer2D->GetMTime())
  {
    // Regenerate since something changed
    this->GenerateTransfer2D();
  }

  return this->Storage->Transfer2D.GetPointer();
}

//-----------------------------------------------------------------------------
void vtkChartTransfer2DEditor::GenerateTransfer2D()
{
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
    const vtkIdType width = static_cast<vtkIdType>(box[3]);
    const vtkIdType height = static_cast<vtkIdType>(box[4]);

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

    for (vtkIdType j = 0; j < height; j++)
      for (vtkIdType i = 0; i < width; i++)
      {
        double color[4];

        color[0] = dataRGB[i];
        color[1] = dataRGB[i + 1];
        color[2] = dataRGB[i + 2];
        color[3] = dataAlpha[i];

        const vtkIdType index = j * width + i;
        transfer->SetTuple(index, color);
      }

    // Cleanup
    delete dataRGB;
    delete dataAlpha;
}


//-----------------------------------------------------------------------------
void vtkChartTransfer2DEditor::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
