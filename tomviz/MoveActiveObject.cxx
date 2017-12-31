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
#include "Utilities.h"

#include "pqView.h"
#include "vtkBoundingBox.h"
#include "vtkBoxRepresentation.h"
#include "vtkBoxWidget2.h"
#include "vtkCommand.h"
#include "vtkDataObject.h"
#include "vtkEvent.h"
#include "vtkEventQtSlotConnect.h"
#include "vtkImageData.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkTrivialProducer.h"
#include "vtkWidgetEvent.h"
#include "vtkWidgetEventTranslator.h"

namespace tomviz {

MoveActiveObject::MoveActiveObject(QObject* p) : Superclass(p)
{
  ActiveObjects& activeObjs = ActiveObjects::instance();
  this->BoxRep->SetPlaceFactor(1.0);
  this->BoxRep->HandlesOff();

  this->BoxWidget->SetTranslationEnabled(1);
  this->BoxWidget->SetScalingEnabled(0);
  this->BoxWidget->SetRotationEnabled(0);
  this->BoxWidget->SetMoveFacesEnabled(0);
  this->BoxWidget->SetRepresentation(this->BoxRep.GetPointer());
  vtkWidgetEventTranslator* translator = this->BoxWidget->GetEventTranslator();
  translator->RemoveTranslation(vtkCommand::LeftButtonPressEvent);
  translator->RemoveTranslation(vtkCommand::LeftButtonReleaseEvent);
  translator->SetTranslation(vtkCommand::LeftButtonPressEvent,
                             vtkEvent::NoModifier, 0, 0, NULL,
                             vtkWidgetEvent::Translate);
  translator->SetTranslation(vtkCommand::LeftButtonReleaseEvent,
                             vtkEvent::NoModifier, 0, 0, NULL,
                             vtkWidgetEvent::EndTranslate);
  this->BoxWidget->SetPriority(1);

  this->connect(&activeObjs, SIGNAL(dataSourceActivated(DataSource*)),
                SLOT(dataSourceActivated(DataSource*)));
  this->connect(&activeObjs, SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(onViewChanged(vtkSMViewProxy*)));
  this->connect(&activeObjs, SIGNAL(moveObjectsModeChanged(bool)),
                SLOT(setMoveEnabled(bool)));
  this->EventLink->Connect(this->BoxWidget.GetPointer(),
                           vtkCommand::InteractionEvent, this,
                           SLOT(interactionEnd(vtkObject*)));
  for (int i = 0; i < 3; ++i) {
    this->DataLocation[i] = 0.0;
  }
  this->MoveEnabled = false;
}

MoveActiveObject::~MoveActiveObject()
{
}

void MoveActiveObject::updateForNewDataSource(DataSource* source)
{
  if (!source) {
    this->BoxWidget->EnabledOff();
    if (this->View) {
      this->View->render();
    }
    return;
  }
  vtkDataObject* data = source->producer()->GetOutputDataObject(0);
  double dataBounds[6];
  vtkImageData::SafeDownCast(data)->GetBounds(dataBounds);
  const double* displayPosition = source->displayPosition();
  for (int i = 0; i < 3; ++i) {
    this->DataLocation[i] = dataBounds[i * 2];
    dataBounds[2 * i] += displayPosition[i];
    dataBounds[2 * i + 1] += displayPosition[i];
  }
  this->BoxRep->PlaceWidget(dataBounds);
  this->BoxWidget->EnabledOn();
  if (this->View) {
    this->View->render();
  }
}

void MoveActiveObject::hideMoveObjectWidget()
{
  this->BoxWidget->EnabledOff();
  if (this->View) {
    this->View->render();
  }
}

void MoveActiveObject::onViewChanged(vtkSMViewProxy* view)
{
  pqView* pqview = tomviz::convert<pqView*>(view);
  if (this->View == pqview) {
    return;
  }

  if (view && view->GetRenderWindow()) {
    this->BoxWidget->SetInteractor(view->GetRenderWindow()->GetInteractor());
    pqview->render();
  } else {
    this->BoxWidget->SetInteractor(nullptr);
    this->BoxWidget->EnabledOff();
  }

  if (this->View) {
    this->View->render();
  }
  this->View = pqview;
}

void MoveActiveObject::interactionEnd(vtkObject* caller)
{
  Q_UNUSED(caller);

  double* boxBounds = this->BoxRep->GetBounds();

  double displayPosition[4];

  for (int i = 0; i < 3; ++i) {
    displayPosition[i] = boxBounds[i * 2] - this->DataLocation[i];
  }

  ActiveObjects::instance().activeDataSource()->setDisplayPosition(
    displayPosition);
}

void MoveActiveObject::setMoveEnabled(bool enabled)
{
  this->MoveEnabled = enabled;
  if (!enabled) {
    this->hideMoveObjectWidget();
  } else {
    this->updateForNewDataSource(ActiveObjects::instance().activeDataSource());
  }
}

void MoveActiveObject::dataSourceActivated(DataSource* ds)
{
  if (this->MoveEnabled) {
    this->updateForNewDataSource(ds);
  }
}
}
