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

#include "vtkTransferFunction2DItem.h"

#include <vtkColorTransferFunction.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRect.h>

vtkTransferFunction2DItem::vtkTransferFunction2DItem()
{
}

vtkTransferFunction2DItem::~vtkTransferFunction2DItem() = default;

vtkStandardNewMacro(vtkTransferFunction2DItem)

  void vtkTransferFunction2DItem::PrintSelf(ostream& os, vtkIndent indent)
{
}

void vtkTransferFunction2DItem::SetColorTransferFunction(
  vtkColorTransferFunction* f)
{
  if (m_colorTransferFunction != f) {
    m_colorTransferFunction = f;
    this->Modified();
  }
}

vtkColorTransferFunction* vtkTransferFunction2DItem::GetColorTransferFunction()
{
  return m_colorTransferFunction;
}

void vtkTransferFunction2DItem::SetOpacityFunction(vtkPiecewiseFunction* f)
{
  if (m_opacityFunction != f) {
    m_opacityFunction = f;
    this->Modified();
  }
}

vtkPiecewiseFunction* vtkTransferFunction2DItem::GetOpacityFunction()
{
  return m_opacityFunction;
}

void vtkTransferFunction2DItem::SetBox(double x, double y, double width,
                                       double height)
{
  m_x = x;
  m_y = y;
  m_width = width;
  m_height = height;
  this->Modified();
}

void vtkTransferFunction2DItem::SetBox(const vtkRectd& box)
{
  m_x = box.GetX();
  m_y = box.GetY();
  m_width = box.GetWidth();
  m_height = box.GetHeight();
  this->Modified();
}

vtkRectd vtkTransferFunction2DItem::GetBox()
{
  return vtkRectd(m_x, m_y, m_width, m_height);
}
