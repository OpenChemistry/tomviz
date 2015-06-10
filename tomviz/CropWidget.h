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
#ifndef tomvizCropWidget_h
#define tomvizCropWidget_h

#include <QWidget>

#include <vtkNew.h>
#include <vtkSmartPointer.h>

class vtkBoxWidget2;
class vtkRenderWindowInteractor;

namespace tomviz
{

class DataSource;

class CropWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  CropWidget(DataSource* source, vtkRenderWindowInteractor* iren,
      QWidget* parent = 0);
  virtual ~CropWidget();

  void getBounds(double bounds[6]);
private:

  vtkNew< vtkBoxWidget2 > boxWidget;
  vtkSmartPointer< vtkRenderWindowInteractor > interactor;

  DataSource* data;
};

}

#endif
