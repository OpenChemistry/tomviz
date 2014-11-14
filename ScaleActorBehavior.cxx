/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

*****************************************************************************/
#include "ScaleActorBehavior.h"


#include "pqApplicationCore.h"
#include "pqServerManagerModel.h"
#include "pqView.h"
#include "vtkAxisActor2D.h"
#include "vtkCallbackCommand.h"
#include "vtkMath.h"
#include "vtkNew.h"
#include "vtkPVRenderView.h"
#include "vtkRenderer.h"
#include "vtkSMRenderViewProxy.h"

namespace TEM
{
static void UpdateScale(vtkObject *caller,
  unsigned long vtkNotUsed(eventId), void *clientData, void *)
{
  vtkAxisActor2D *axis = static_cast<vtkAxisActor2D*>(clientData);
  vtkRenderer *ren = static_cast<vtkRenderer *>(caller);

  double pos1[3];
  pos1[0] = axis->GetPoint1()[0];
  pos1[1] = axis->GetPoint1()[1];
  pos1[2] = 0.0;
  double pos2[3];
  pos2[0] = axis->GetPoint2()[0];
  pos2[1] = axis->GetPoint2()[1];
  pos2[2] = 0.0;

  ren->NormalizedViewportToView(pos1[0], pos1[1], pos1[2]);
  ren->NormalizedViewportToView(pos2[0], pos2[1], pos2[2]);
  ren->ViewToWorld(pos1[0], pos1[1], pos1[2]);
  ren->ViewToWorld(pos2[0], pos2[1], pos2[2]);
  double distance = vtkMath::Distance2BetweenPoints(pos1,pos2);

  int scale = floor(log10(distance)-0.7);
  switch (scale)
    {
    case -7:
    case -8:
    case -9:
      axis->SetTitle("size in nanometers");
      axis->SetRange(0,distance*1000000000.0);
      break;
    case -4:
    case -5:
    case -6:
      axis->SetTitle("size in micrometers");
      axis->SetRange(0,distance*1000000.0);
      break;
    case -1:
    case -2:
    case -3:
      axis->SetTitle("size in millimeters");
      axis->SetRange(0,distance*1000.0);
      break;
    case 2:
    case 1:
    case 0:
      axis->SetTitle("size in meters");
      axis->SetRange(0,distance);
      break;
    case 5:
    case 4:
    case 3:
      axis->SetTitle("size in kilometers");
      axis->SetRange(0,distance/1000.0);
      break;
    default:
      axis->SetTitle("out of range");
      axis->SetRange(0,1.0);
      break;
    }
}

//-----------------------------------------------------------------------------
ScaleActorBehavior::ScaleActorBehavior(QObject* parentObject)
  : Superclass(parentObject)
{
  pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  this->connect(smmodel, SIGNAL(viewAdded(pqView*)), SLOT(viewAdded(pqView*)));
}

//-----------------------------------------------------------------------------
ScaleActorBehavior::~ScaleActorBehavior()
{
}

//-----------------------------------------------------------------------------
void ScaleActorBehavior::viewAdded(pqView* view)
{
  if (vtkSMRenderViewProxy* viewProxy = vtkSMRenderViewProxy::SafeDownCast(view->getProxy()))
    {
    vtkRenderer* ren = vtkPVRenderView::SafeDownCast(viewProxy->GetClientSideObject())
                       ->GetNonCompositedRenderer();
    Q_ASSERT(ren);

    vtkNew<vtkAxisActor2D> axis;
    axis->SetPoint1(0.6,0.1);
    axis->SetPoint2(0.9,0.1);
    axis->SetTitle("size in meters");
    axis->SetLabelFormat("%3.0f");

    vtkNew<vtkCallbackCommand> cbc;
    cbc->SetCallback(UpdateScale);
    cbc->SetClientData(axis.GetPointer());
    ren->AddObserver(vtkCommand::StartEvent, cbc.GetPointer());
    ren->AddActor(axis.GetPointer());
    }
}

}
