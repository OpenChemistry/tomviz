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

#include "vtkSmartVolumeMapper.h"
#include "vtkBoxWidget2.h"
#include "vtkBoxRepresentation.h"
#include "vtkImageData.h"
#include "vtkTrivialProducer.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkVolume.h"
#include "vtkVolumeProperty.h"
#include "vtkSMSourceProxy.h"
#include "vtkRenderWindow.h"
#include "vtkRendererCollection.h"
#include "vtkRenderer.h"

#include <QHBoxLayout>

#include "DataSource.h"

namespace tomviz
{
CropWidget::CropWidget(DataSource* source, vtkRenderWindowInteractor* iren,
    QWidget* parent)
  : QWidget(parent), interactor(iren), data(source)
{
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    source->producer()->GetClientSideObject());
  vtkImageData* imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  // copied from the BoxWidget2 tests and slightly modified
  vtkNew< vtkBoxRepresentation > boxRep;
  boxRep->SetPlaceFactor(1.0);
  boxRep->PlaceWidget(imageData->GetBounds());
  boxRep->HandlesOn();

  this->boxWidget->SetTranslationEnabled(0);
  this->boxWidget->SetScalingEnabled(0);
  this->boxWidget->SetRotationEnabled(0);
  this->boxWidget->SetMoveFacesEnabled(1);
  this->boxWidget->SetInteractor(iren);
  this->boxWidget->SetRepresentation(boxRep.GetPointer());
  this->boxWidget->SetPriority(1);
  this->boxWidget->EnabledOn();

  // These lines make it show up... it still doesn't work
  iren->GetRenderWindow()->GetRenderers()->GetFirstRenderer()->AddViewProp(boxRep.GetPointer());
  iren->GetRenderWindow()->Render();
}

CropWidget::~CropWidget()
{
  // how to remove?
  this->boxWidget->SetInteractor(NULL);
  this->interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer()->AddViewProp(
      this->boxWidget->GetRepresentation());
  this->interactor->GetRenderWindow()->Render();
}

}
