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
#include "vtkCustomPiecewiseControlPointsItem.h"

#include "vtkContextMouseEvent.h"
#include "vtkContextScene.h"
#include "vtkObjectFactory.h"

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkCustomPiecewiseControlPointsItem);

//-----------------------------------------------------------------------------
vtkCustomPiecewiseControlPointsItem::vtkCustomPiecewiseControlPointsItem()
{
}

//-----------------------------------------------------------------------------
vtkCustomPiecewiseControlPointsItem::~vtkCustomPiecewiseControlPointsItem()
{
}

//-----------------------------------------------------------------------------
bool vtkCustomPiecewiseControlPointsItem::MouseButtonPressEvent(const vtkContextMouseEvent & mouse)
{
  // Skip if the control button is pressed
  int modifiers = mouse.GetModifiers();
  if (modifiers & vtkContextMouseEvent::CONTROL_MODIFIER)
  {
    return false;
  }

  return this->Superclass::MouseButtonPressEvent(mouse);
}

//-----------------------------------------------------------------------------
bool vtkCustomPiecewiseControlPointsItem::MouseDoubleClickEvent(const vtkContextMouseEvent & vtkNotUsed(mouse))
{
  return false;
}
