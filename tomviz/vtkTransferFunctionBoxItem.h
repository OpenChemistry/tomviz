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
#ifndef tomvizvtkTransferFunctionBoxItem_h
#define tomvizvtkTransferFunctionBoxItem_h

#include <vtkControlPointsItem.h>
#include <vtkNew.h> // For vtkNew<> members
#include <vtkRect.h> // For vtkRect

/**
 * \brief Box representation of a transfer function.
 *
 * Holds a color/opacity transfer functions. The box or any of its corners
 * can be draged to either change its position or size.  The parent chart
 * uses its defined rectangle and transfer functions to raster a 2D transfer
 * function. This item is intended to be used as a selection item in 
 * vtkChartTransfer2DEditor.
 */

class vtkColorTransferFunction;
class vtkPiecewiseFunction;
class vtkPoints2D;

class vtkTransferFunctionBoxItem : public vtkControlPointsItem
{
public:
  static vtkTransferFunctionBoxItem* New();
  vtkTransferFunctionBoxItem(const vtkTransferFunctionBoxItem&) = delete;
  void operator=(const vtkTransferFunctionBoxItem&) = delete;

  vtkTypeMacro(vtkTransferFunctionBoxItem, vtkControlPointsItem)
  void PrintSelf(ostream &os, vtkIndent indent) VTK_OVERRIDE;

  //@{
  /**
   * Transfer functions represented by this box item.
   */ 
  void SetColorFunction(vtkColorTransferFunction* function);
  vtkGetObjectMacro(ColorFunction, vtkColorTransferFunction)
  void SetOpacityFunction(vtkPiecewiseFunction* function);
  vtkGetObjectMacro(OpacityFunction, vtkPiecewiseFunction)
  //@}

  /**
   * Returns the curren box as [x0, y0, width, height].
   */
  const vtkRectd& GetBox();

protected:
  vtkTransferFunctionBoxItem();
  ~vtkTransferFunctionBoxItem() override;

  vtkIdType AddPoint(const double x, const double y);
  vtkIdType AddPoint(double* pos) VTK_OVERRIDE;

  /**
   *  Box corners are ordered as follows:
   *  3 ----- 2
   *  |       |
   *  0 ----- 1
   */
  enum BoxCorners
  {
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    TOP_RIGHT,
    TOP_LEFT
  };

  /**
   * This method does nothing as this item has a fixed number of points (4).
   */
  vtkIdType RemovePoint(double* pos) VTK_OVERRIDE;

  vtkIdType GetNumberOfPoints() const VTK_OVERRIDE;

  void GetControlPoint(vtkIdType index, double* point) const VTK_OVERRIDE;

  vtkMTimeType GetControlPointsMTime();

  void SetControlPoint(vtkIdType index, double* point) VTK_OVERRIDE;

  void emitEvent(unsigned long event, void* params = 0) VTK_OVERRIDE;

  void MovePoint(const vtkIdType pointId, const double deltaX,
    const double deltaY);

  void DragBox(const double deltaX, const double deltaY);

  void DragCorner(const vtkIdType cornerId, const double* delta);

  bool Paint(vtkContext2D* painter) VTK_OVERRIDE;

  /**
   * Returns true if the supplied x, y coordinate is within the bounds of
   * the box or any of the control points.
   */
  bool Hit(const vtkContextMouseEvent& mouse) VTK_OVERRIDE;

  //@{
  /**
   * \brief Interaction overrides.
   * The box item can be dragged around the chart area by clicking within
   * the box and moving the cursor.  The size of the box can be manipulated by
   * clicking on the control points and moving them. No key events are currently
   * reimplemented.
   */
  bool MouseButtonPressEvent(const vtkContextMouseEvent &mouse) VTK_OVERRIDE;
  bool MouseButtonReleaseEvent(const vtkContextMouseEvent &mouse) VTK_OVERRIDE;
  bool MouseDoubleClickEvent(const vtkContextMouseEvent &mouse) VTK_OVERRIDE;
  bool MouseMoveEvent(const vtkContextMouseEvent &mouse) VTK_OVERRIDE;
  bool KeyPressEvent(const vtkContextKeyEvent &key) VTK_OVERRIDE;
  bool KeyReleaseEvent(const vtkContextKeyEvent &key) VTK_OVERRIDE;
  //@}

private:
  /**
   * Custom method to clamp point positions to valid bounds (chart bounds).  A custom
   * method was required given that ControlPoints::ClampValidPos() appears to have
   * bug where it does not not clamp to bounds[2,3].  The side effects of overriding
   * that behavior are unclear so for now this custom method is used.
   */
  void ClampToValidPosition(double pos[2]);

  /**
   * Predicate to check whether pointA crosses pointB in either axis after
   * displacing pontA by deltaA.
   */
  bool ArePointsCrossing(const vtkIdType pointA, const double* deltaA,
    const vtkIdType pointB);

  /**
   * Points move independently. In order to keep the box rigid when dragging it
   * outside of the edges it is first checked whether it stays within bounds.
   */
  bool BoxIsWithinBounds(const double deltaX, const double deltaY);

  vtkNew<vtkPoints2D> BoxPoints;
  vtkRectd Box;
  vtkPiecewiseFunction* OpacityFunction = nullptr;
  vtkColorTransferFunction* ColorFunction = nullptr;
};
#endif // tomvizvtkTransferFunctionBoxItem_h
