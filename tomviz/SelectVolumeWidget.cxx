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

#include "SelectVolumeWidget.h"
#include "ui_SelectVolumeWidget.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkBoundingBox.h>
#include <vtkBoxWidget2.h>
#include <vtkBoxRepresentation.h>
#include <vtkCommand.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkInteractorObserver.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <QHBoxLayout>
#include <QSettings>

#include "ActiveObjects.h"
#include "CropOperator.h"
#include "DataSource.h"

namespace tomviz
{

class SelectVolumeWidget::CWInternals
{
public:
  vtkNew< vtkBoxWidget2 > boxWidget;
  vtkSmartPointer< vtkRenderWindowInteractor > interactor;
  vtkNew<vtkEventQtSlotConnect> eventLink;

  Ui::SelectVolumeWidget ui;
  int dataExtent[6];
  double dataOrigin[3];
  double dataSpacing[3];
  vtkBoundingBox dataBoundingBox;

  void bounds(int bs[6]) const
  {
    int index = 0;
    bs[index++] = this->ui.startX->value();
    bs[index++] = this->ui.endX->value();
    bs[index++] = this->ui.startY->value();
    bs[index++] = this->ui.endY->value();
    bs[index++] = this->ui.startZ->value();
    bs[index++] = this->ui.endZ->value();
  }

  void blockSpinnerSignals(bool block)
  {
    ui.startX->blockSignals(block);
    ui.startY->blockSignals(block);
    ui.startZ->blockSignals(block);
    ui.endX->blockSignals(block);
    ui.endY->blockSignals(block);
    ui.endZ->blockSignals(block);
  }
};

SelectVolumeWidget::SelectVolumeWidget(const double origin[3],
                                       const double spacing[3],
                                       const int extent[6],
                                       const int currentVolume[6],
                                       QWidget* p)
  : Superclass(p), Internals(new SelectVolumeWidget::CWInternals())
{
  vtkRenderWindowInteractor *iren =
    ActiveObjects::instance().activeView()->GetRenderWindow()->GetInteractor();
  this->Internals->interactor = iren;

  for (int i = 0; i < 3; ++i)
  {
    this->Internals->dataOrigin[i] = origin[i];
    this->Internals->dataSpacing[i] = spacing[i];
    this->Internals->dataExtent[2 * i] = extent[2 * i];
    this->Internals->dataExtent[2 * i + 1] = extent[2 * i + 1];
  }

  double bounds[6];
  for (int i = 0; i < 6; ++i)
  {
    bounds[i] = this->Internals->dataOrigin[i >> 1] +
      this->Internals->dataSpacing[i >> 1] * this->Internals->dataExtent[i];
  }
  vtkNew< vtkBoxRepresentation > boxRep;
  boxRep->SetPlaceFactor(1.0);
  boxRep->PlaceWidget(bounds);
  boxRep->HandlesOn();

  this->Internals->boxWidget->SetTranslationEnabled(1);
  this->Internals->boxWidget->SetScalingEnabled(1);
  this->Internals->boxWidget->SetRotationEnabled(0);
  this->Internals->boxWidget->SetMoveFacesEnabled(1);
  this->Internals->boxWidget->SetInteractor(iren);
  this->Internals->boxWidget->SetRepresentation(boxRep.GetPointer());
  this->Internals->boxWidget->SetPriority(1);
  this->Internals->boxWidget->EnabledOn();

  this->Internals->eventLink->Connect(this->Internals->boxWidget.GetPointer(), vtkCommand::InteractionEvent,
                           this, SLOT(interactionEnd(vtkObject*)));

  iren->GetRenderWindow()->Render();

  Ui::SelectVolumeWidget& ui = this->Internals->ui;
  ui.setupUi(this);

  double e[6];
  std::copy(extent, extent+6, e);
  this->Internals->dataBoundingBox.SetBounds(e);

  // Set ranges and default values
  ui.startX->setRange(extent[0], extent[1]);
  ui.startX->setValue(currentVolume[0]);
  ui.startY->setRange(extent[2], extent[3]);
  ui.startY->setValue(currentVolume[2]);
  ui.startZ->setRange(extent[4], extent[5]);
  ui.startZ->setValue(currentVolume[4]);

  ui.endX->setRange(extent[0], extent[1]);
  ui.endX->setValue(currentVolume[1]);
  ui.endY->setRange(extent[2], extent[3]);
  ui.endY->setValue(currentVolume[3]);
  ui.endZ->setRange(extent[4], extent[5]);
  ui.endZ->setValue(currentVolume[5]);

  this->connect(ui.startX, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.startY, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.startZ, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.endX, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.endY, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.endZ, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  // force through the current values pulled from the operator and set above
  this->valueChanged();
}

SelectVolumeWidget::~SelectVolumeWidget()
{
  this->Internals->boxWidget->SetInteractor(NULL);
  this->Internals->interactor->GetRenderWindow()->Render();
}

void SelectVolumeWidget::interactionEnd(vtkObject *caller)
{
  Q_UNUSED(caller);

  double* boxBounds = this->Internals->boxWidget->GetRepresentation()->GetBounds();

  double* spacing = this->Internals->dataSpacing;
  double* origin = this->Internals->dataOrigin;
  double dataBounds[6];

  int dim = 0;
  for (int i = 0; i < 6; i++)
  {
    dataBounds[i] =  (boxBounds[i] - origin[dim]) / spacing[dim] ;
    dim += i % 2 ? 1 : 0;
  }

  this->updateBounds(dataBounds);
}

void SelectVolumeWidget::updateBounds(int* bounds)
{
  double* spacing = this->Internals->dataSpacing;
  double* origin = this->Internals->dataOrigin;
  double newBounds[6];

  int dim = 0;
  for (int i = 0; i < 6; i++)
  {
    newBounds[i] =  (bounds[i] * spacing[dim]) + origin[dim];
    dim += i % 2 ? 1 : 0;
  }

  this->Internals->boxWidget->GetRepresentation()->PlaceWidget(newBounds);
  this->Internals->interactor->GetRenderWindow()->Render();
}

//-----------------------------------------------------------------------------
void SelectVolumeWidget::getExtentOfSelection(int extent[6])
{
  this->Internals->bounds(extent);
}

//-----------------------------------------------------------------------------
void SelectVolumeWidget::getBoundsOfSelection(double bounds[6])
{
  double* boxBounds = this->Internals->boxWidget->GetRepresentation()->GetBounds();
  for (int i = 0; i < 6; ++i)
  {
    bounds[i] = boxBounds[i];
  }
}

//-----------------------------------------------------------------------------
void SelectVolumeWidget::updateBounds(double *newBounds)
{
  Ui::SelectVolumeWidget& ui = this->Internals->ui;

  this->Internals->blockSpinnerSignals(true);

  vtkBoundingBox newBoundingBox(newBounds);

  if (this->Internals->dataBoundingBox.Intersects(newBoundingBox))
  {
    ui.startX->setValue(vtkMath::Round(newBounds[0]));
    ui.startY->setValue(vtkMath::Round(newBounds[2]));
    ui.startZ->setValue(vtkMath::Round(newBounds[4]));

    ui.endX->setValue(vtkMath::Round(newBounds[1]));
    ui.endY->setValue(vtkMath::Round(newBounds[3]));
    ui.endZ->setValue(vtkMath::Round(newBounds[5]));
  }
  // If there is no intersection use data extent
  else
  {
    ui.startX->setValue(this->Internals->dataExtent[0]);
    ui.startY->setValue(this->Internals->dataExtent[2]);
    ui.startZ->setValue(this->Internals->dataExtent[4]);

    ui.endX->setValue(this->Internals->dataExtent[1]);
    ui.endY->setValue(this->Internals->dataExtent[3]);
    ui.endZ->setValue(this->Internals->dataExtent[5]);

  }

  this->Internals->blockSpinnerSignals(false);
}

//-----------------------------------------------------------------------------
void SelectVolumeWidget::valueChanged()
{
  int cropVolume[6];

  this->Internals->bounds(cropVolume);

  this->updateBounds(cropVolume);
}

}
