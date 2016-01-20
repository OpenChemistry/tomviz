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

#include "MoveActiveObject.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "Module.h"
#include "ModuleManager.h"

#include "vtkBoundingBox.h"
#include "vtkBoxRepresentation.h"
#include "vtkBoxWidget2.h"
#include "vtkCommand.h"
#include "vtkDataObject.h"
#include "vtkEventQtSlotConnect.h"
#include "vtkImageData.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkTrivialProducer.h"

namespace tomviz
{

MoveActiveObject::MoveActiveObject(QObject *p)
  : Superclass(p)
{
  ActiveObjects &activeObjs = ActiveObjects::instance();
  this->BoxRep->SetPlaceFactor(1.0);
  this->BoxRep->HandlesOn();

  this->BoxWidget->SetTranslationEnabled(1);
  this->BoxWidget->SetScalingEnabled(0);
  this->BoxWidget->SetRotationEnabled(0);
  this->BoxWidget->SetMoveFacesEnabled(0);
  this->BoxWidget->SetRepresentation(this->BoxRep.GetPointer());
  this->BoxWidget->SetPriority(1);

  this->connect(&activeObjs, SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateForNewDataSource(DataSource*)));
  this->connect(&activeObjs, SIGNAL(dataSourceActivated(DataSource*)),
                SLOT(updateForNewDataSource(DataSource*)));
  this->connect(&activeObjs, SIGNAL(moduleChanged(Module*)),
                SLOT(hideMoveObjectWidget()));
  this->connect(&activeObjs, SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(onViewChanged(vtkSMViewProxy*)));
  this->EventLink->Connect(this->BoxWidget.GetPointer(), vtkCommand::InteractionEvent,
                           this, SLOT(interactionEnd(vtkObject*)));
  for (int i = 0; i < 3; ++i)
  {
    this->DataLocation[i] = 0.0;
  }

}

MoveActiveObject::~MoveActiveObject()
{
}

void MoveActiveObject::updateForNewDataSource(DataSource *source)
{
  vtkRenderWindowInteractor *iren = this->BoxWidget->GetInteractor();
  if (!source)
  {
    this->BoxWidget->EnabledOff();
    if (iren)
    {
      iren->GetRenderWindow()->Render();
    }
    return;
  }
  vtkDataObject *data = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject())->GetOutputDataObject(0);
  double dataBounds[6];
  vtkImageData::SafeDownCast(data)->GetBounds(dataBounds);
  const double *displayPosition = source->displayPosition();
  for (int i = 0; i < 3; ++i)
  {
    this->DataLocation[i] = dataBounds[i * 2];
    dataBounds[2 * i] += displayPosition[i];
    dataBounds[2 * i + 1] += displayPosition[i];
  }
  this->BoxRep->PlaceWidget(dataBounds);
  this->BoxWidget->EnabledOn();
  if (iren)
  {
    iren->GetRenderWindow()->Render();
  }
}

void MoveActiveObject::hideMoveObjectWidget()
{
  vtkRenderWindowInteractor *iren = this->BoxWidget->GetInteractor();
  this->BoxWidget->EnabledOff();
  if (iren)
  {
    iren->GetRenderWindow()->Render();
  }
}

void MoveActiveObject::onViewChanged(vtkSMViewProxy *view)
{
  if (view)
  {
    vtkRenderWindowInteractor *iren =
      view->GetRenderWindow()->GetInteractor();
    this->BoxWidget->SetInteractor(iren);
    view->GetRenderWindow()->Render();
  }
  else
  {
    vtkRenderWindowInteractor *iren = this->BoxWidget->GetInteractor();
    this->BoxWidget->SetInteractor(nullptr);
    this->BoxWidget->EnabledOff();
    if (iren)
    {
      iren->GetRenderWindow()->Render();
    }
  }
}

void MoveActiveObject::interactionEnd(vtkObject *caller)
{
  Q_UNUSED(caller);

  double* boxBounds = this->BoxRep->GetBounds();

  double displayPosition[3];

  for (int i = 0; i < 3; ++i)
  {
    displayPosition[i] = boxBounds[i * 2] - this->DataLocation[i];
  }

  ActiveObjects::instance().activeDataSource()->setDisplayPosition(displayPosition);
}

}
