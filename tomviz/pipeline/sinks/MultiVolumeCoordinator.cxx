/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MultiVolumeCoordinator.h"

#include "VolumeSink.h"

#include <QDebug>
#include <QSet>

#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageData.h>
#include <vtkLight.h>
#include <vtkMultiVolume.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLGPUVolumeRayCastMapper.h>
#include <vtkPVRenderView.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkSMViewProxy.h>
#include <vtkVolume.h>
#include <vtkVolumeMapper.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace tomviz {
namespace pipeline {

/// The shared mapper, which knows when its GPU resources have been released
/// so the coordinator can retire it (see the class comment in the header).
class MultiVolumeMapper : public vtkOpenGLGPUVolumeRayCastMapper
{
public:
  static MultiVolumeMapper* New();
  vtkTypeMacro(MultiVolumeMapper, vtkOpenGLGPUVolumeRayCastMapper);

  void ReleaseGraphicsResources(vtkWindow* window) override
  {
    this->Superclass::ReleaseGraphicsResources(window);
    this->Released = true;
  }

  /// True once anything released this mapper's resources. It never goes
  /// back to false: the mapper is replaced instead.
  bool GetReleased() const { return this->Released; }

private:
  bool Released = false;
};

vtkStandardNewMacro(MultiVolumeMapper);

namespace {

QHash<vtkSMViewProxy*, MultiVolumeCoordinator*>& registry()
{
  static QHash<vtkSMViewProxy*, MultiVolumeCoordinator*> coordinators;
  return coordinators;
}

double smallestSpacing(vtkImageData* image)
{
  double spacing[3] = { 1.0, 1.0, 1.0 };
  image->GetSpacing(spacing);
  const double smallest = std::min(
    std::min(std::abs(spacing[0]), std::abs(spacing[1])), std::abs(spacing[2]));
  return smallest > 0.0 ? smallest : 1.0;
}

} // namespace

MultiVolumeCoordinator* MultiVolumeCoordinator::find(vtkSMViewProxy* view)
{
  if (!view) {
    return nullptr;
  }
  auto& coordinators = registry();
  auto it = coordinators.find(view);
  if (it == coordinators.end()) {
    return nullptr;
  }
  // A destroyed view can hand its address to a new one. The weak pointer
  // went null with the old view, so a mismatch means this entry is stale.
  auto* coordinator = it.value();
  if (coordinator->m_view.GetPointer() != view) {
    coordinators.erase(it);
    coordinator->deleteLater();
    return nullptr;
  }
  return coordinator;
}

MultiVolumeCoordinator* MultiVolumeCoordinator::forView(vtkSMViewProxy* view)
{
  if (!view) {
    return nullptr;
  }
  if (auto* existing = find(view)) {
    return existing;
  }
  auto* coordinator = new MultiVolumeCoordinator(view);
  registry().insert(view, coordinator);
  return coordinator;
}

MultiVolumeCoordinator::MultiVolumeCoordinator(vtkSMViewProxy* view)
  : m_view(view), m_viewKey(view)
{
  m_renderView = vtkPVRenderView::SafeDownCast(view->GetClientSideView());

  m_mapper = makeMapper();
  m_multiVolume->SetMapper(m_mapper);

  // Exactly what vtkOpenGLGPUVolumeRayCastMapper calls default lighting:
  // one switched-on headlight at full intensity. Anything else and its
  // multi-volume shader shades nothing.
  m_headlight->SetLightTypeToHeadlight();
  m_headlight->SetIntensity(1.0);
  m_headlight->SwitchOn();

  // A GL context can be recreated under a live view (re-parenting the
  // widget does it), which releases every prop's resources. Catch that
  // before the frame that follows draws with the released mapper.
  if (auto* renderer = m_renderView ? m_renderView->GetRenderer() : nullptr) {
    m_renderObserver->SetClientData(this);
    m_renderObserver->SetCallback(
      [](vtkObject*, unsigned long, void* clientData, void*) {
        auto* self = static_cast<MultiVolumeCoordinator*>(clientData);
        if (self->m_active) {
          self->ensureFreshMapper();
        }
      });
    m_renderObserverId =
      renderer->AddObserver(vtkCommand::StartEvent, m_renderObserver);
  }
}

MultiVolumeCoordinator::~MultiVolumeCoordinator()
{
  auto& coordinators = registry();
  if (coordinators.value(m_viewKey) == this) {
    coordinators.remove(m_viewKey);
  }
  if (m_renderView) {
    if (auto* renderer = m_renderView->GetRenderer()) {
      if (m_renderObserverId) {
        renderer->RemoveObserver(m_renderObserverId);
      }
    }
    if (m_active) {
      m_renderView->RemovePropFromRenderer(m_multiVolume);
      useHeadlight(false);
    }
  }
}

vtkSmartPointer<MultiVolumeMapper> MultiVolumeCoordinator::makeMapper()
{
  auto mapper = vtkSmartPointer<MultiVolumeMapper>::New();
  // The multi-volume path composites only; the mapper's other settings
  // that the sinks expose (jittering, scattering) either always hold or
  // are not supported there. Scattering stays at its zero default, which
  // is also what keeps the unbounded shadow frames VolumeSink guards
  // against from ever being drawn through this mapper.
  mapper->SetBlendMode(vtkVolumeMapper::COMPOSITE_BLEND);
  mapper->UseJitteringOn();
  // Each port's input carries its own active scalars (see attach), so the
  // one array selection the mapper has is "whatever is active".
  mapper->SetScalarModeToUsePointData();
  mapper->SetAutoAdjustSampleDistances(1);
  return mapper;
}

void MultiVolumeCoordinator::ensureFreshMapper()
{
  if (!m_mapper->GetReleased()) {
    return;
  }
  m_mapper = makeMapper();
  m_multiVolume->SetMapper(m_mapper);
  for (auto* member : m_members) {
    // The inputs live on the mapper, so every port is connected anew.
    m_entries[member].input = nullptr;
    attach(member);
  }
  refreshSettings();
}

void MultiVolumeCoordinator::useHeadlight(bool on)
{
  auto* renderer = m_renderView ? m_renderView->GetRenderer() : nullptr;
  if (!renderer) {
    return;
  }
  if (on) {
    // A view already lit some other way is left alone: with the kit off
    // the renderer runs on its own headlight, which is what we want.
    if (m_lightKitSuspended || !m_renderView->GetUseLightKit()) {
      return;
    }
    // Done on the view object rather than its proxy's UseLight property,
    // so nothing about this detour is written into a saved state.
    m_renderView->SetUseLightKit(false);
    renderer->AddLight(m_headlight);
    m_lightKitSuspended = true;
  } else if (m_lightKitSuspended) {
    renderer->RemoveLight(m_headlight);
    m_renderView->SetUseLightKit(true);
    m_lightKitSuspended = false;
  }
}

vtkSMViewProxy* MultiVolumeCoordinator::view() const
{
  return m_view;
}

vtkMultiVolume* MultiVolumeCoordinator::multiVolume() const
{
  return m_multiVolume;
}

vtkGPUVolumeRayCastMapper* MultiVolumeCoordinator::mapper() const
{
  return m_mapper;
}

bool MultiVolumeCoordinator::isMember(const VolumeSink* sink) const
{
  return m_entries.contains(const_cast<VolumeSink*>(sink));
}

VolumeSink* MultiVolumeCoordinator::lead() const
{
  for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
    if (it.value().port == 0) {
      return it.key();
    }
  }
  return nullptr;
}

int MultiVolumeCoordinator::lowestFreePort() const
{
  for (int port = 0; port < kMaximumMembers; ++port) {
    bool taken = false;
    for (const auto& entry : m_entries) {
      if (entry.port == port) {
        taken = true;
        break;
      }
    }
    if (!taken) {
      return port;
    }
  }
  return -1;
}

bool MultiVolumeCoordinator::addMember(VolumeSink* sink)
{
  if (!sink) {
    return false;
  }
  if (isMember(sink)) {
    refreshInput(sink);
    return true;
  }
  const int port = lowestFreePort();
  if (port < 0) {
    qWarning("MultiVolumeCoordinator: a view can render at most %d volumes "
             "together; \"%s\" is drawn on its own and may not composite "
             "correctly where it overlaps the others.",
             kMaximumMembers, qPrintable(sink->label()));
    return false;
  }

  m_members.append(sink);
  Member member;
  member.port = port;
  m_entries.insert(sink, member);
  attach(sink);
  refreshSettings();

  if (!m_active && m_members.size() >= kMinimumMembers) {
    activate();
  }
  notifyMembers();
  return true;
}

void MultiVolumeCoordinator::removeMember(VolumeSink* sink)
{
  if (!isMember(sink)) {
    return;
  }
  const Member gone = m_entries.take(sink);
  m_members.removeOne(sink);
  detach(gone.port);

  // Port 0 has to stay occupied: the mapper reads its property for the
  // shading of the whole set and its input to decide whether there is
  // anything to draw at all. Promote the earliest remaining member.
  if (gone.port == 0 && !m_members.isEmpty()) {
    auto* next = m_members.first();
    auto& entry = m_entries[next];
    detach(entry.port);
    entry.port = 0;
    entry.input = nullptr; // force a fresh connection on the new port
    attach(next);
  }

  if (m_active && m_members.size() < kMinimumMembers) {
    deactivate();
  }
  refreshSettings();
  notifyMembers(sink);

  if (m_members.isEmpty()) {
    auto& coordinators = registry();
    if (coordinators.value(m_viewKey) == this) {
      coordinators.remove(m_viewKey);
    }
    deleteLater();
  }
}

void MultiVolumeCoordinator::attach(VolumeSink* sink)
{
  auto it = m_entries.find(sink);
  if (it == m_entries.end()) {
    return;
  }
  auto& entry = it.value();
  auto* source = sink->renderedImage();
  if (!source) {
    return;
  }
  const QString array = sink->renderedArrayName();

  std::array<double, 3> origin{};
  std::array<double, 3> spacing{};
  std::array<int, 6> extent{};
  source->GetOrigin(origin.data());
  source->GetSpacing(spacing.data());
  source->GetExtent(extent.data());

  // A shallow copy shares the arrays, so edits to the voxels reach the
  // mapper through their MTime as they would from the original. What it
  // does not share is the geometry, hence the comparison here: a metadata
  // edit (spacing, origin) has to produce a new copy to be seen.
  const bool unchanged = entry.input && entry.source == source &&
                         entry.array == array && entry.origin == origin &&
                         entry.spacing == spacing && entry.extent == extent;
  if (!unchanged) {
    vtkNew<vtkImageData> copy;
    copy->ShallowCopy(source);
    if (!array.isEmpty()) {
      copy->GetPointData()->SetActiveScalars(array.toUtf8().constData());
    }
    entry.input = copy;
    entry.source = source;
    entry.array = array;
    entry.origin = origin;
    entry.spacing = spacing;
    entry.extent = extent;
    m_mapper->SetInputDataObject(entry.port, copy);
  }
  m_multiVolume->SetVolume(sink->volumeProp(), entry.port);
}

void MultiVolumeCoordinator::detach(int port)
{
  if (port < 0) {
    return;
  }
  m_multiVolume->RemoveVolume(port);
  if (m_mapper->GetNumberOfInputConnections(port) > 0) {
    m_mapper->RemoveInputConnection(port, 0);
  }
}

void MultiVolumeCoordinator::refreshInput(VolumeSink* sink)
{
  if (isMember(sink)) {
    attach(sink);
  }
}

void MultiVolumeCoordinator::refreshSettings()
{
  auto* leadSink = lead();
  m_mapper->SetComputeNormalFromOpacity(leadSink && leadSink->smoothNormals()
                                          ? 1
                                          : 0);

  // One mapper, one set of planes: a plane clipping any member clips them
  // all. The shader takes at most six, so a plane two members share (a
  // Clip sink feeding both) is added once.
  m_mapper->RemoveAllClippingPlanes();
  QSet<vtkPlane*> seen;
  for (auto* member : m_members) {
    auto* planes = member->clippingPlanes();
    if (!planes) {
      continue;
    }
    planes->InitTraversal();
    while (auto* plane = planes->GetNextItem()) {
      // A member's exploded-view slab planes cut its own slabs, which
      // the shared path does not draw; they must not cut the set.
      if (!seen.contains(plane) && !member->isExplodedSlabPlane(plane)) {
        seen.insert(plane);
        m_mapper->AddClippingPlane(plane);
      }
    }
  }

  // Fine sampling (a label map's half-voxel step) is a property of the
  // march, so one member asking for it sets it for the set.
  double fineStep = std::numeric_limits<double>::max();
  for (auto* member : m_members) {
    if (!member->fineSampling()) {
      continue;
    }
    if (auto* image = member->renderedImage()) {
      fineStep = std::min(fineStep, 0.5 * smallestSpacing(image));
    }
  }
  if (fineStep < std::numeric_limits<double>::max()) {
    m_mapper->SetAutoAdjustSampleDistances(0);
    m_mapper->SetSampleDistance(static_cast<float>(fineStep));
    m_mapper->SetImageSampleDistance(1.0f);
  } else {
    m_mapper->SetAutoAdjustSampleDistances(1);
  }
}

void MultiVolumeCoordinator::activate()
{
  if (m_active) {
    return;
  }
  m_active = true;
  // Deactivating released the previous mapper's resources (see below).
  ensureFreshMapper();
  if (m_renderView) {
    m_renderView->AddPropToRenderer(m_multiVolume);
    useHeadlight(true);
  }
  emit activeChanged(true);
}

void MultiVolumeCoordinator::deactivate()
{
  if (!m_active) {
    return;
  }
  m_active = false;
  if (m_renderView) {
    // Removing the prop also releases the mapper's textures, so a view
    // back to a single volume holds no GPU memory for the set. That
    // release is also what retires the mapper; the next activation
    // starts from a fresh one.
    m_renderView->RemovePropFromRenderer(m_multiVolume);
    useHeadlight(false);
  }
  emit activeChanged(false);
}

void MultiVolumeCoordinator::notifyMembers(VolumeSink* also)
{
  // Members pull their own props out of the renderer (or put them back)
  // and refresh their panels from what the coordinator now says.
  const auto members = m_members;
  for (auto* member : members) {
    member->applyMultiVolumeState();
  }
  if (also && !members.contains(also)) {
    also->applyMultiVolumeState();
  }
}

} // namespace pipeline
} // namespace tomviz
