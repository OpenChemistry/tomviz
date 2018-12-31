/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

/**
 * @class vtkChartTransfer2DEditor
 * @brief Generates and edits a 2D transfer function (vtkImageData) based
 * on its current vtkTransferFunctionBoxItems.
 *
 * \todo Only supports a single vtkTransferFunctionBoxItem.  The rasterization
 * code can be called in a loop over multiple items, but this currently only
 * supports a single item.
 *
 * \todo Currently rasterization occurs in Utilities. In order to support
 * additional shapes (besides rectangular boxes), much of this functionality
 * should be moved into the item class.
 */

#ifndef vtkChartTransfer2DEditor_h
#define vtkChartTransfer2DEditor_h

#include <vtkChartHistogram2D.h>

#include <vtkNew.h>
#include <vtkSmartPointer.h>

class vtkCallbackCommand;
class vtkImageData;
class vtkTransferFunctionBoxItem;

class vtkChartTransfer2DEditor : public vtkChartHistogram2D
{
public:
  static vtkChartTransfer2DEditor* New();
  void PrintSelf(ostream& os, vtkIndent indent) override;
  vtkTypeMacro(vtkChartTransfer2DEditor, vtkChartHistogram2D)

    /**
     * Set the vtkImageData on which to raster the 2D transfer function
     * and the vtkRectd to store its box coordinates into.
     */
    void SetTransfer2D(vtkImageData* transfer2D, vtkRectd* box);

  /**
   * Events from added BoxItems (vtkCommand::SelectionChangedEvent) are
   * observed in order to trigger Transfer2D generation. A default position
   * /size is set in the BoxItem.
   */
  vtkIdType AddFunction(vtkTransferFunctionBoxItem* boxItem);

  /**
   * Allocates and clears Transfer2D to be updated, the dimenions of the
   * histogram (e.g. number of bins) are used as dimensions for the transfer
   * function. Calls Utilities::rasterTransferFunction2DBox for the actual
   * update. It invokes vtkCommand::EndEvent after the update, this signal
   * should be caught by handlers using the generated 2DTF.
   */
  void GenerateTransfer2D();

  void SetInputData(vtkImageData* data, vtkIdType z = 0) VTK_OVERRIDE;

protected:
  vtkChartTransfer2DEditor();
  ~vtkChartTransfer2DEditor() override;

  vtkSmartPointer<vtkImageData> Transfer2D;
  vtkNew<vtkCallbackCommand> Callback;
  vtkRectd* Transfer2DBox;
  vtkRectd DummyBox;

  vtkPlot* GetPlot(vtkIdType index) override;

  static void OnBoxItemModified(vtkObject* caller, unsigned long eid,
                                void* clientData, void* callData);

  /**
   * Update bounds of each box item in the chart.
   */
  void UpdateItemsBounds(const double xMin, const double xMax,
                         const double yMin, const double yMax);

  /**
   * This chart only supports plots of type vtkTransferFunctionBoxItem.
   */
  using vtkChartXY::AddPlot;
  vtkIdType AddPlot(vtkPlot* plot) override;

  /**
   * Positions the item in the center of the chart.
   */
  void SetDefaultBoxPosition(vtkSmartPointer<vtkTransferFunctionBoxItem> item,
                             const double xRange[2], const double yRange[2]);

  bool IsInitialized();

private:
  vtkChartTransfer2DEditor(const vtkChartTransfer2DEditor&) = delete;
  void operator=(const vtkChartTransfer2DEditor&) = delete;
};

#endif // vtkChartTransfer2DEditor_h
