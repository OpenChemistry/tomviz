/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MoveActiveObject.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Utilities.h"
#include "vtkCustomBoxRepresentation.h"

#include <pqView.h>

#include <vtkActor.h>
#include <vtkBoundingBox.h>
#include <vtkBoxWidget2.h>
#include <vtkCommand.h>
#include <vtkDataObject.h>
#include <vtkEvent.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkImageData.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>
#include <vtkWidgetEvent.h>
#include <vtkWidgetEventTranslator.h>

namespace tomviz {

MoveActiveObject::MoveActiveObject(QObject* p) : Superclass(p)
{
  ActiveObjects& activeObjs = ActiveObjects::instance();
  this->BoxRep->SetPlaceFactor(1.0);
  this->BoxRep->HandlesOn();
  this->BoxRep->SetHandleSize(10);

  this->BoxWidget->SetRepresentation(this->BoxRep.GetPointer());
  this->BoxWidget->SetPriority(1);

  this->connect(&activeObjs, SIGNAL(dataSourceActivated(DataSource*)),
                SLOT(dataSourceActivated(DataSource*)));
  this->connect(&activeObjs, SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(onViewChanged(vtkSMViewProxy*)));
  connect(&activeObjs, &ActiveObjects::moveObjectsModeChanged, this,
          &MoveActiveObject::setTransformType);
  this->EventLink->Connect(this->BoxWidget.GetPointer(),
                           vtkCommand::InteractionEvent, this,
                           SLOT(interactionEnd(vtkObject*)));
  this->EventLink->Connect(this->BoxWidget.GetPointer(),
                           vtkCommand::EndInteractionEvent, this,
                           SLOT(interactionEnd(vtkObject*)));
  for (int i = 0; i < 3; ++i) {
    this->DataLocation[i] = 0.0;
  }

  this->setTransformType(TransformType::None);
}

MoveActiveObject::~MoveActiveObject() {}

void MoveActiveObject::updateForNewDataSource(DataSource* source)
{
  if (!source) {
    this->BoxWidget->EnabledOff();
    if (this->View) {
      this->View->render();
    }
    return;
  }

  this->onDataPropertiesChanged();
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

  auto ds = ActiveObjects::instance().activeDataSource();

  switch (this->Transform) {
    case (TransformType::Translate): {
      double displayPosition[3];

      for (int i = 0; i < 3; ++i) {
        displayPosition[i] = boxBounds[i * 2] - this->DataLocation[i];
      }

      ds->setDisplayPosition(displayPosition);

      break;
    }
    case (TransformType::Resize): {
      int extent[6];
      double spacing[3];
      double displayPosition[3];
      ds->getExtent(extent);

      for (int i = 0; i < 3; ++i) {
        auto length = boxBounds[i * 2 + 1] - boxBounds[i * 2];
        auto size = extent[i * 2 + 1] - extent[i * 2];
        spacing[i] = length / size;
        displayPosition[i] = boxBounds[i * 2] - this->DataLocation[i];
      }

      ds->setSpacing(spacing);
      ds->setDisplayPosition(displayPosition);
      break;
    }
    case (TransformType::Rotate): {
      break;
    }
    case (TransformType::None):
    default: {
      break;
    }
  }
}

void MoveActiveObject::setTransformType(TransformType transform)
{
  this->Transform = transform;

  switch (this->Transform) {
    case (TransformType::Translate): {
      this->BoxWidget->RotationEnabledOff();
      this->BoxWidget->ScalingEnabledOff();
      this->BoxWidget->MoveFacesEnabledOff();
      this->BoxWidget->TranslationEnabledOn();
      this->BoxRep->HandlesOff();
      this->BoxRep->GetHandle()[6]->SetVisibility(1);
      this->updateForNewDataSource(
        ActiveObjects::instance().activeDataSource());
      this->BoxWidget->EnabledOn();
      break;
    }
    case (TransformType::Resize): {
      this->BoxWidget->RotationEnabledOff();
      this->BoxWidget->ScalingEnabledOff();
      this->BoxWidget->MoveFacesEnabledOn();
      this->BoxWidget->TranslationEnabledOff();
      this->BoxRep->HandlesOn();
      this->BoxRep->GetHandle()[6]->SetVisibility(0);
      this->updateForNewDataSource(
        ActiveObjects::instance().activeDataSource());
      this->BoxWidget->EnabledOn();
      break;
    }
    case (TransformType::Rotate): {
      break;
    }
    case (TransformType::None):
    default: {
      this->BoxWidget->EnabledOff();
      break;
    }
  }

  if (this->View) {
    this->View->render();
  }
}

void MoveActiveObject::dataSourceActivated(DataSource* ds)
{
  if (this->currentDataSource) {
    this->disconnect(this->currentDataSource);
  }

  this->currentDataSource = ds;

  if (ds) {
    connect(ds, &DataSource::displayPositionChanged, this,
            &MoveActiveObject::onDataPositionChanged);
    connect(ds, &DataSource::dataPropertiesChanged, this,
            &MoveActiveObject::onDataPropertiesChanged);
  }

  if (this->Transform != TransformType::None) {
    this->updateForNewDataSource(ds);
  }
}

void MoveActiveObject::onDataPropertiesChanged()
{
  auto source = this->currentDataSource;
  if (!source) {
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

  if (this->View) {
    this->View->render();
  }
}

void MoveActiveObject::onDataPositionChanged(double, double, double)
{
  this->onDataPropertiesChanged();
}

} // namespace tomviz
