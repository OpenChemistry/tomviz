/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkChartTransfer2DEditor.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

/**
 * @class   vtkChartTransfer2DEditor
 * @brief  
 *
*/

#ifndef vtkChartTransfer2DEditor_h
#define vtkChartTransfer2DEditor_h

#include <vtkChartHistogram2D.h>


class vtkImageData;
class vtkTransferFunctionBoxItem;

class vtkChartTransfer2DEditor : public vtkChartHistogram2D
{
public:
  static vtkChartTransfer2DEditor* New();
  void PrintSelf(ostream &os, vtkIndent indent) override;
  vtkTypeMacro(vtkChartTransfer2DEditor, vtkChartHistogram2D)

  /**
   * Get generated 2D transfer function.
   */
  vtkImageData* GetTransfer2D();

protected:
  vtkChartTransfer2DEditor();
  ~vtkChartTransfer2DEditor() override;

  class Private;
  Private* Storage;

  vtkPlot* GetPlot(vtkIdType index) override;

private:
  void GenerateTransfer2D();

  void RasterBoxItem(vtkTransferFunctionBoxItem* boxItem);

  vtkChartTransfer2DEditor(const vtkChartTransfer2DEditor &) = delete;
  void operator=(const vtkChartTransfer2DEditor &) = delete;
};

#endif //vtkChartTransfer2DEditor_h
