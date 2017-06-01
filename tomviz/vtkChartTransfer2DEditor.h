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
 * @class vtkChartTransfer2DEditor
 * @brief Generates and edits a 2D transfer function (vtkImageData) based
 * on its current vtkTransferFunctionBoxItems.
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
  void PrintSelf(ostream& os, vtkIndent indent) override;
  vtkTypeMacro(vtkChartTransfer2DEditor, vtkChartHistogram2D)

    /**
     * Set the vtkImageData on which to raster the 2D transfer function.
     */
    void SetTransfer2D(vtkImageData* transfer2D);

  vtkIdType AddPlot(vtkPlot* plot) override;

protected:
  vtkChartTransfer2DEditor();
  ~vtkChartTransfer2DEditor() override;

  class Private;
  Private* Storage;

  vtkPlot* GetPlot(vtkIdType index) override;

  static void OnBoxItemModified(vtkObject* caller, unsigned long eid,
                                void* clientData, void* callData);

private:
  void GenerateTransfer2D();

  void RasterBoxItem(vtkTransferFunctionBoxItem* boxItem);

  vtkChartTransfer2DEditor(const vtkChartTransfer2DEditor&) = delete;
  void operator=(const vtkChartTransfer2DEditor&) = delete;
};

#endif // vtkChartTransfer2DEditor_h
