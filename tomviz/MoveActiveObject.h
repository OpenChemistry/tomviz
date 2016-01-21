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
#ifndef tomvizMoveActiveObject_h
#define tomvizMoveActiveObject_h

#include <QObject>
#include <QPoint>

#include "vtkNew.h"
#include "vtkVector.h"

class pqDataRepresentation;
class vtkBoxRepresentation;
class vtkBoxWidget2;
class vtkEventQtSlotConnect;
class vtkObject;
class vtkSMViewProxy;

namespace tomviz
{
class DataSource;

class MoveActiveObject : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;
public:
  MoveActiveObject(QObject *parent);
  ~MoveActiveObject();

public slots:
  void updateForNewDataSource(DataSource *newDS);
  void hideMoveObjectWidget();
  void onViewChanged(vtkSMViewProxy *newView);
  void interactionEnd(vtkObject *obj);

private:
  Q_DISABLE_COPY(MoveActiveObject)
  vtkNew<vtkBoxRepresentation> BoxRep;
  vtkNew<vtkBoxWidget2> BoxWidget;
  vtkNew<vtkEventQtSlotConnect> EventLink;
  vtkVector3d DataLocation;
};
}
#endif
