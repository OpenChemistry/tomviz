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
#ifndef vtkTransferFunction2DItem_h
#define vtkTransferFunction2DItem_h

#include <QObject>

#include <vtkObject.h>
#include <vtkSmartPointer.h>

#include <vector>

class vtkColorTransferFunction;
class vtkPiecewiseFunction;
class vtkRectd;

class vtkTransferFunction2DItem : public vtkObject
{
public:
  static vtkTransferFunction2DItem* New();
  vtkTransferFunction2DItem(const vtkTransferFunction2DItem&) = delete;
  void operator=(const vtkTransferFunction2DItem&) = delete;

  vtkTypeMacro(vtkTransferFunction2DItem, vtkObject)

    void PrintSelf(ostream& os, vtkIndent indent) override;

  void SetColorTransferFunction(vtkColorTransferFunction*);
  vtkColorTransferFunction* GetColorTransferFunction();

  void SetOpacityFunction(vtkPiecewiseFunction*);
  vtkPiecewiseFunction* GetOpacityFunction();

  void SetBox(double x, double y, double width, double height);
  void SetBox(const vtkRectd& rect);
  double GetX() const { return m_x; }
  double GetY() const { return m_y; }
  double GetWidth() const { return m_width; }
  double GetHeight() const { return m_height; }
  vtkRectd GetBox();

protected:
  vtkTransferFunction2DItem();
  ~vtkTransferFunction2DItem();

private:
  double m_x, m_y, m_width, m_height;
  vtkSmartPointer<vtkColorTransferFunction> m_colorTransferFunction;
  vtkSmartPointer<vtkPiecewiseFunction> m_opacityFunction;
};

#endif
