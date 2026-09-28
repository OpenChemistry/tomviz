/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ClippingRangeBehavior.h"

#include <pqApplicationCore.h>
#include <pqRenderView.h>
#include <pqServerManagerModel.h>

#include <vtkCommand.h>
#include <vtkPVRenderView.h>
#include <vtkRenderer.h>
#include <vtkSMViewProxy.h>
#include <vtkWeakPointer.h>

namespace tomviz {

namespace {

// Weak, so a renderer that outlives its view never calls into a dead one
class SynchronizeBounds : public vtkCommand
{
public:
  static SynchronizeBounds* New() { return new SynchronizeBounds; }

  vtkWeakPointer<vtkPVRenderView> View;

  void Execute(vtkObject*, unsigned long, void*) override
  {
    // Recomputes the bounds from the visible props and resets the
    // clipping range to them, before the renderer applies the camera
    if (this->View) {
      this->View->SynchronizeGeometryBounds();
    }
  }
};

} // namespace

ClippingRangeBehavior::ClippingRangeBehavior(QObject* parent)
  : QObject(parent)
{
  auto* model = pqApplicationCore::instance()->getServerManagerModel();
  for (auto* view : model->findItems<pqRenderView*>()) {
    watchView(view);
  }
  connect(model, &pqServerManagerModel::viewAdded, this,
          &ClippingRangeBehavior::watchView);
}

void ClippingRangeBehavior::watchView(pqView* view)
{
  if (auto* renderView = qobject_cast<pqRenderView*>(view)) {
    watch(renderView->getViewProxy());
  }
}

void ClippingRangeBehavior::watch(vtkSMViewProxy* view)
{
  auto* pvView =
    view ? vtkPVRenderView::SafeDownCast(view->GetClientSideView()) : nullptr;
  auto* renderer = pvView ? pvView->GetRenderer() : nullptr;
  if (!renderer) {
    return;
  }
  vtkNew<SynchronizeBounds> command;
  command->View = pvView;
  renderer->AddObserver(vtkCommand::StartEvent, command);
}

} // namespace tomviz
