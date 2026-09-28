/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ImageViewerModeBehavior.h"

#include "ActiveObjects.h"
#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineUtils.h"
#include "pipeline/PortType.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/SliceSink.h"

#include <pqApplicationCore.h>
#include <pqRenderView.h>
#include <pqServerManagerModel.h>

#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkPVRenderView.h>
#include <vtkRenderWindow.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSmartPointer.h>

#include <QList>
#include <QPointer>

#include <cmath>

namespace tomviz {

using pipeline::LegacyModuleSink;
using pipeline::OutputPort;
using pipeline::SliceSink;

/// What entering 2D changed in a view, so leaving can put it back.
struct ImageViewerModeBehavior::State
{
  vtkSmartPointer<vtkCamera> camera;
  QList<QPointer<LegacyModuleSink>> hiddenSinks;
  QPointer<SliceSink> slice;
  bool sliceWasVisible = true;
  bool sliceShowedArrow = true;
};

namespace {

/// The port upstream of a sink, looking through the SinkGroupNode a
/// sink usually hangs off.
OutputPort* feedingPort(pipeline::SinkNode* sink)
{
  if (sink->inputPorts().isEmpty()) {
    return nullptr;
  }
  auto* link = sink->inputPorts()[0]->link();
  if (!link) {
    return nullptr;
  }
  auto* from = link->from();
  auto* group = qobject_cast<pipeline::SinkGroupNode*>(from->node());
  if (!group) {
    return from;
  }
  int idx = group->outputPorts().indexOf(from);
  if (idx < 0 || idx >= group->inputPorts().size()) {
    return from;
  }
  auto* groupLink = group->inputPorts()[idx]->link();
  return groupLink ? groupLink->from() : nullptr;
}

/// With several slices in the view, the one fed by the tip port; with
/// one, that one. Falls back to a visible slice, then the first.
SliceSink* chooseSlice(const QList<SliceSink*>& slices, OutputPort* tipPort)
{
  if (slices.isEmpty()) {
    return nullptr;
  }
  if (tipPort) {
    for (auto* slice : slices) {
      if (feedingPort(slice) == tipPort) {
        return slice;
      }
    }
  }
  for (auto* slice : slices) {
    if (slice->visibility()) {
      return slice;
    }
  }
  return slices.first();
}

/// A new slice in the view, hanging off the tip port; nullptr when there
/// is no tip port, it carries no volume, or the view can't host it.
SliceSink* createSlice(pipeline::Pipeline* pip, vtkSMRenderViewProxy* proxy,
                       OutputPort* tipPort)
{
  if (!tipPort) {
    return nullptr;
  }
  auto* slice = new SliceSink();
  slice->setLabel("Slice");
  auto* input = slice->inputPorts()[0];
  if (!pipeline::isPortTypeCompatible(tipPort->type(),
                                      input->acceptedTypes()) ||
      !slice->initialize(proxy)) {
    delete slice;
    return nullptr;
  }
  pip->addNode(slice);
  pip->createLink(pipeline::sinkAttachPort(pip, tipPort, input), input);
  pip->executeWhenIdle();
  return slice;
}

bool volumeBounds(const pipeline::PortData& data, double bounds[6])
{
  if (!data.isValid() || !pipeline::isVolumeType(data.type())) {
    return false;
  }
  auto vol = data.value<pipeline::VolumeDataPtr>();
  if (!vol || !vol->isValid()) {
    return false;
  }
  vol->imageData()->GetBounds(bounds);
  return true;
}

/// Bounds of what the slice shows: its own last data, else what feeds
/// it, else the tip port (a slice created just now has consumed nothing
/// yet, and its group's passthrough is empty until the pipeline runs).
bool sliceBounds(SliceSink* slice, OutputPort* tipPort, double bounds[6])
{
  auto vol = slice->volumeData();
  if (vol && vol->isValid()) {
    vol->imageData()->GetBounds(bounds);
    return true;
  }
  if (auto* port = feedingPort(slice)) {
    if (volumeBounds(port->data(), bounds)) {
      return true;
    }
  }
  return tipPort && volumeBounds(tipPort->data(), bounds);
}

/// Where the camera looks and which way is up for a slice, as close to
/// the current camera as the slice allows: the camera looks down the
/// slice normal from the side it is already on, and up is the in-plane
/// axis nearest its current up. When the camera gives no hint (it looks
/// edge-on, or its up is the normal) the defaults are x right and y up
/// for XY, x right and z up for XZ, y right and z up for YZ. Returns the
/// axis the camera looks along and sets @a upAxis to the one pointing
/// up, or -1 for both with a custom plane.
int lookAlong(SliceSink* slice, vtkCamera* camera, double look[3],
              double up[3], int& upAxis)
{
  double current[3], currentUp[3];
  camera->GetDirectionOfProjection(current);
  camera->GetViewUp(currentUp);
  const double hint = 1e-6;

  int axis = -1;
  double normal[3] = { 0, 0, 0 };
  switch (slice->direction()) {
    case SliceSink::XY:
      axis = 2, upAxis = 1, normal[2] = -1;
      break;
    case SliceSink::XZ:
      axis = 1, upAxis = 2, normal[1] = 1;
      break;
    case SliceSink::YZ:
      axis = 0, upAxis = 2, normal[0] = -1;
      break;
    default:
      upAxis = -1;
      slice->planeNormal(normal);
      if (vtkMath::Normalize(normal) == 0.0) {
        normal[0] = 0, normal[1] = 0, normal[2] = 1;
      }
      vtkMath::MultiplyScalar(normal, -1.0);
      break;
  }

  // The side the camera is on
  double sign = vtkMath::Dot(normal, current) < -hint ? -1.0 : 1.0;
  for (int i = 0; i < 3; ++i) {
    look[i] = sign * normal[i];
  }

  if (axis >= 0) {
    // The in-plane axis, either way round, nearest the current up
    double best = hint;
    int bestAxis = -1;
    for (int i = 0; i < 3; ++i) {
      if (i != axis && std::abs(currentUp[i]) > best) {
        best = std::abs(currentUp[i]);
        bestAxis = i;
      }
    }
    up[0] = up[1] = up[2] = 0;
    if (bestAxis >= 0) {
      upAxis = bestAxis;
      up[upAxis] = currentUp[upAxis] < 0 ? -1 : 1;
    } else {
      up[upAxis] = 1;
    }
    return axis;
  }

  // Custom plane: the current up, flattened onto the plane
  double along = vtkMath::Dot(currentUp, look);
  for (int i = 0; i < 3; ++i) {
    up[i] = currentUp[i] - along * look[i];
  }
  if (vtkMath::Normalize(up) < hint) {
    // Any up vector not parallel to the normal will do.
    if (std::abs(look[2]) < 0.9) {
      up[0] = 0, up[1] = 0, up[2] = 1;
    } else {
      up[0] = 0, up[1] = 1, up[2] = 0;
    }
  }
  return -1;
}

/// Tighten the parallel scale so the slice fills the viewport along its
/// narrower dimension, instead of ResetCamera's bounding-sphere fit.
void fitParallelScale(vtkSMRenderViewProxy* proxy, const double bounds[6],
                      int axis, int upAxis)
{
  int* size = proxy->GetRenderWindow()->GetSize();
  double w = size[0];
  double h = size[1];
  if (w <= 0 || h <= 0) {
    return;
  }

  double lengths[3] = { bounds[1] - bounds[0], bounds[3] - bounds[2],
                        bounds[5] - bounds[4] };
  double bw = lengths[3 - axis - upAxis];
  double bh = lengths[upAxis];
  if (bw <= 0 || bh <= 0) {
    return;
  }

  double viewAspect = w / h;
  double boundsAspect = bw / bh;
  double scale = viewAspect >= boundsAspect ? bh / 2 : bw / 2 / viewAspect;
  proxy->GetActiveCamera()->SetParallelScale(scale);
}

void aimCamera(vtkSMRenderViewProxy* proxy, SliceSink* slice,
               OutputPort* tipPort)
{
  double look[3], up[3];
  int upAxis = -1;
  int axis = lookAlong(slice, proxy->GetActiveCamera(), look, up, upAxis);
  proxy->ResetActiveCameraToDirection(look[0], look[1], look[2], up[0],
                                      up[1], up[2]);
  double bounds[6];
  if (sliceBounds(slice, tipPort, bounds)) {
    proxy->ResetCamera(bounds);
    if (axis >= 0) {
      fitParallelScale(proxy, bounds, axis, upAxis);
    }
  } else {
    proxy->ResetCamera();
  }
}

} // namespace

ImageViewerModeBehavior::ImageViewerModeBehavior(QObject* parentObject)
  : QObject(parentObject)
{
  auto* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  for (auto* view : smmodel->findItems<pqRenderView*>()) {
    watch(view);
  }
  connect(smmodel, &pqServerManagerModel::viewAdded, this,
          &ImageViewerModeBehavior::watch);
  connect(smmodel, &pqServerManagerModel::preViewRemoved, this,
          [this](pqView* view) {
            m_states.erase(qobject_cast<pqRenderView*>(view));
          });
}

ImageViewerModeBehavior::~ImageViewerModeBehavior() = default;

bool ImageViewerModeBehavior::isActive(pqRenderView* view) const
{
  return m_states.count(view) > 0;
}

SliceSink* ImageViewerModeBehavior::slice(pqRenderView* view) const
{
  auto it = m_states.find(view);
  return it == m_states.end() ? nullptr : it->second->slice.data();
}

void ImageViewerModeBehavior::watch(pqView* view)
{
  auto* renderView = qobject_cast<pqRenderView*>(view);
  if (!renderView) {
    return;
  }
  // pqRenderView emits this for every change of the InteractionMode
  // property, whoever made it: the toolbar toggle, the View menu, or a
  // loaded state.
  connect(renderView, qOverload<int>(&pqRenderView::updateInteractionMode),
          this, [this, renderView](int mode) {
            onInteractionModeChanged(renderView, mode);
          });
}

void ImageViewerModeBehavior::onInteractionModeChanged(pqRenderView* view,
                                                       int mode)
{
  // Selection and zoom-to-box are transient and hand back to the mode
  // they interrupted, so only 2D and 3D themselves count.
  if (mode == vtkPVRenderView::INTERACTION_MODE_2D) {
    enter(view);
  } else if (mode == vtkPVRenderView::INTERACTION_MODE_3D) {
    leave(view);
  }
}

void ImageViewerModeBehavior::enter(pqRenderView* view)
{
  if (isActive(view)) {
    return;
  }
  auto* pip = ActiveObjects::instance().pipeline();
  auto* proxy = view->getRenderViewProxy();
  if (!pip || !proxy) {
    return;
  }

  QList<LegacyModuleSink*> others;
  QList<SliceSink*> slices;
  for (auto* node : pip->nodes()) {
    auto* sink = qobject_cast<LegacyModuleSink*>(node);
    if (!sink || sink->view() != proxy) {
      continue;
    }
    if (auto* sliceSink = qobject_cast<SliceSink*>(sink)) {
      slices.append(sliceSink);
    } else {
      others.append(sink);
    }
  }

  // Nothing selected yet (a dataset just loaded, say): the first
  // branch's tip, as ActiveObjects itself falls back to.
  auto* tipPort = ActiveObjects::instance().activeTipOutputPort();
  if (!tipPort) {
    tipPort = pipeline::findTipOutputPort(pip, nullptr);
  }
  auto* slice = chooseSlice(slices, tipPort);
  bool created = false;
  if (!slice) {
    slice = createSlice(pip, proxy, tipPort);
    if (!slice) {
      // Nothing to look at: leave the view as it is, just with 2D
      // interaction.
      return;
    }
    created = true;
  }

  auto state = std::make_unique<State>();
  state->camera = vtkSmartPointer<vtkCamera>::New();
  // DeepCopy: the snapshot must not share matrices with the live camera
  state->camera->DeepCopy(proxy->GetActiveCamera());
  for (auto* sink : others) {
    if (sink->visibility()) {
      state->hiddenSinks.append(sink);
      sink->setVisibility(false);
    }
  }
  state->slice = slice;
  state->sliceWasVisible = !created && slice->visibility();
  state->sliceShowedArrow = slice->showArrow();
  slice->setVisibility(true);
  // The plane-moving arrow points at the camera here and its drag
  // would fight the 2D pan.
  slice->setShowArrow(false);

  aimCamera(proxy, slice, tipPort);
  m_states[view] = std::move(state);
  view->render();
}

void ImageViewerModeBehavior::leave(pqRenderView* view)
{
  auto it = m_states.find(view);
  if (it == m_states.end()) {
    return;
  }
  std::unique_ptr<State> state = std::move(it->second);
  m_states.erase(it);

  for (const auto& sink : state->hiddenSinks) {
    if (sink) {
      sink->setVisibility(true);
    }
  }
  if (state->slice) {
    state->slice->setShowArrow(state->sliceShowedArrow);
    // A slice created for the viewer stays in the pipeline, hidden, so
    // the next 2D toggle finds it where the user left it.
    if (!state->sliceWasVisible) {
      state->slice->setVisibility(false);
    }
  }
  if (auto* proxy = view->getRenderViewProxy()) {
    proxy->GetActiveCamera()->DeepCopy(state->camera);
  }
  view->render();
}

} // namespace tomviz
