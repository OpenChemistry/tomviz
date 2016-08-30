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
#ifndef tomvizvtkCustomPiecewiseControlPointsItem_h
#define tomvizvtkCustomPiecewiseControlPointsItem_h

#include <vtkPiecewiseControlPointsItem.h>

class vtkContextMouseEvent;

// Special control points item class that overrides the MouseDoubleClickEvent()
// event handler to do nothing.
class vtkCustomPiecewiseControlPointsItem : public vtkPiecewiseControlPointsItem
{
public:
  vtkTypeMacro(
    vtkCustomPiecewiseControlPointsItem,
    vtkPiecewiseControlPointsItem) static vtkCustomPiecewiseControlPointsItem* New();

  // Override to ignore button presses if the control modifier key is pressed.
  bool MouseButtonPressEvent(const vtkContextMouseEvent& mouse) override;

  // Override to avoid catching double-click events
  bool MouseDoubleClickEvent(const vtkContextMouseEvent& mouse) override;

protected:
  vtkCustomPiecewiseControlPointsItem();
  virtual ~vtkCustomPiecewiseControlPointsItem();

  // Utility function to determine whether a position is near the piecewise
  // function.
  bool PointNearPiecewiseFunction(const double pos[2]);

private:
  vtkCustomPiecewiseControlPointsItem(
    const vtkCustomPiecewiseControlPointsItem&); // Not implemented.
  void operator=(
    const vtkCustomPiecewiseControlPointsItem&); // Not implemented.
};

#endif // tomvizvtkCustomPiecewiseControlPointsItem_h
