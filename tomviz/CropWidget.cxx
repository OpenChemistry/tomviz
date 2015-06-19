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

#include "CropWidget.h"

#include <vtkSmartVolumeMapper.h>
#include <vtkBoxWidget2.h>
#include <vtkBoxRepresentation.h>
#include <vtkCommand.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkInteractorObserver.h>
#include <vtkImageData.h>
#include <vtkTrivialProducer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkSMSourceProxy.h>
#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkRenderer.h>

#include <QHBoxLayout>

#include "DataSource.h"

namespace tomviz
{

class CropWidget::CWInternals
{
public:
  vtkNew< vtkBoxWidget2 > boxWidget;
  vtkSmartPointer< vtkRenderWindowInteractor > interactor;
  vtkNew<vtkEventQtSlotConnect> eventLink;
  DataSource* dataSource;
  vtkImageData* imageData;
};

CropWidget::CropWidget(DataSource* source, vtkRenderWindowInteractor* iren,
    QObject* p)
  : QObject(p), Internals(new CropWidget::CWInternals())
{
  this->Internals->interactor = iren;
  this->Internals->dataSource = source;

  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    source->producer()->GetClientSideObject());
  this->Internals->imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  vtkNew< vtkBoxRepresentation > boxRep;
  boxRep->SetPlaceFactor(1.0);
  boxRep->PlaceWidget(this->Internals->imageData->GetBounds());
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
}

CropWidget::~CropWidget()
{
  this->Internals->boxWidget->SetInteractor(NULL);
  this->Internals->interactor->GetRenderWindow()->Render();
}

void CropWidget::interactionEnd(vtkObject *caller)
{
  Q_UNUSED(caller);

  double* boxBounds = this->Internals->boxWidget->GetRepresentation()->GetBounds();

  double* spacing = this->Internals->imageData->GetSpacing();
  double* origin = this->Internals->imageData->GetOrigin();
  double dataBounds[6];

  int dim = 0;
  for (int i=0; i<6; i++)
    {
    dataBounds[i] =  (boxBounds[i] - origin[dim]) / spacing[dim] ;
    dim += i % 2 ? 1 : 0;
    }

  emit this->bounds(dataBounds);
}

void CropWidget::updateBounds(int* boxBounds)
{
  double* spacing = this->Internals->imageData->GetSpacing();
  double* origin = this->Internals->imageData->GetOrigin();
  double newBounds[6];

  int dim = 0;
  for (int i=0; i<6; i++)
    {
    newBounds[i] =  (boxBounds[i] * spacing[dim]) + origin[dim];
    dim += i % 2 ? 1 : 0;
    }

  this->Internals->boxWidget->GetRepresentation()->PlaceWidget(newBounds);
  this->Internals->interactor->GetRenderWindow()->Render();
}

}
