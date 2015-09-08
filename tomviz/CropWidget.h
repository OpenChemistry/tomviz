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

#include "EditOperatorWidget.h"
#include <QScopedPointer>

class vtkObject;

namespace tomviz
{

class CropOperator;

class CropWidget : public EditOperatorWidget
{
  Q_OBJECT
  typedef EditOperatorWidget Superclass;

public:
  CropWidget(CropOperator *source,
             QWidget* parent = 0);
  virtual ~CropWidget();

  void getBounds(double bounds[6]);

  virtual void applyChangesToOperator();

private slots:
  void interactionEnd(vtkObject* caller);
  void valueChanged();
  void updateBounds(int* bounds);
  void updateBounds(double *bounds);

private:
  class CWInternals;
  QScopedPointer<CWInternals> Internals;

};

}

#endif
