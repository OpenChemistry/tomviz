/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

*****************************************************************************/
#include "ScaleActorBehavior.h"

#include <pqApplicationCore.h>
#include <pqServerManagerModel.h>
#include <pqView.h>
#include <vtkPVRenderView.h>
#include <vtkSMRenderViewProxy.h>

#include <vtkAxisActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkNew.h>
#include <vtkRenderer.h>
#include <vtkVector.h>
#include <vtkVectorOperators.h>


namespace tomviz {

static void UpdateScale(vtkObject* caller, unsigned long, void* clientData,
                        void*)
{
  auto axis = static_cast<vtkAxisActor2D*>(clientData);
  auto ren = static_cast<vtkRenderer*>(caller);

  vtkVector3d pos1(axis->GetPoint1()[0], axis->GetPoint1()[1], 0);
  vtkVector3d pos2(axis->GetPoint2()[0], axis->GetPoint2()[1], 0);

  ren->NormalizedViewportToView(pos1[0], pos1[1], pos1[2]);
  ren->NormalizedViewportToView(pos2[0], pos2[1], pos2[2]);
  ren->ViewToWorld(pos1[0], pos1[1], pos1[2]);
  ren->ViewToWorld(pos2[0], pos2[1], pos2[2]);

  double distance = vtkVector3d(pos2 - pos1).Norm();

  int scale = floor(log10(distance) - 0.7);
  // cout << "Scale: " << scale << ", Distance: " << distance << endl;
  switch (scale) {
    case -7:
    case -8:
    case -9:
      axis->SetTitle("nm");
      axis->SetRange(0, distance * 1e9);
      break;
    case -4:
    case -5:
    case -6:
      axis->SetTitle("microns");
      axis->SetRange(0, distance * 1e6);
      break;
    case -1:
    case -2:
    case -3:
      axis->SetTitle("mm");
      axis->SetRange(0, distance * 1e3);
      break;
    case 2:
    case 1:
    case 0:
      axis->SetTitle("m");
      axis->SetRange(0, distance);
      break;
    case 5:
    case 4:
    case 3:
      axis->SetTitle("km");
      axis->SetRange(0, distance * 1e-3);
      break;
    default:
      axis->SetTitle("out of range");
      axis->SetRange(0, 1.0);
      break;
  }
}

ScaleActorBehavior::ScaleActorBehavior(QObject* parentObject)
  : QObject(parentObject)
{
  pqServerManagerModel* smmodel =
    pqApplicationCore::instance()->getServerManagerModel();
  connect(smmodel, SIGNAL(viewAdded(pqView*)), SLOT(viewAdded(pqView*)));
}

void ScaleActorBehavior::viewAdded(pqView* view)
{
  if (auto viewProxy =
        vtkSMRenderViewProxy::SafeDownCast(view->getProxy())) {
    auto ren =
      vtkPVRenderView::SafeDownCast(viewProxy->GetClientSideObject())
        ->GetNonCompositedRenderer();
    Q_ASSERT(ren);

    vtkNew<vtkAxisActor2D> axis;
    axis->SetPoint1(0.70, 0.1);
    axis->SetPoint2(0.95, 0.1);
    axis->SetTitle("m");
    axis->SetLabelFormat("%3.1f");
    axis->SetRulerMode(1);
    axis->SetNumberOfLabels(2);
    axis->SetAdjustLabels(0);

    vtkNew<vtkCallbackCommand> cbc;
    cbc->SetCallback(UpdateScale);
    cbc->SetClientData(axis.GetPointer());
    ren->AddObserver(vtkCommand::StartEvent, cbc.Get());
    ren->AddActor(axis.GetPointer());
  }
}
}
