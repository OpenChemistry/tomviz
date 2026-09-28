/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LegacyModuleSink.h"

#include "InputPort.h"
#include "Link.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "Utilities.h"
#include "data/VolumeData.h"

#include <QJsonArray>

#include <vector>

#include <vtkColorTransferFunction.h>
#include <vtkDataArray.h>
#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkPVRenderView.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyManager.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkWeakPointer.h>

namespace tomviz {
namespace pipeline {

namespace {

// Apply a colorOpacityMap JSON ({colors, colorSpace, points}) to the
// client-side VTK objects behind a ParaView color-map SM proxy, then
// push the client-side edits back up to the proxy properties.
//
// The sink reads its scalar opacity via opacityMap()->GetClientSideObject()
// — the PWF of the ScalarOpacityFunction sub-proxy. That is not
// necessarily the same object as the CTF's internal ScalarOpacityFunction,
// so we apply the opacity points to it explicitly in addition to the
// CTF deserialize.
void applyColorMapJson(vtkSMProxy* cmap, const QJsonObject& json)
{
  if (!cmap) {
    return;
  }

  auto* disc = vtkDiscretizableColorTransferFunction::SafeDownCast(
    cmap->GetClientSideObject());
  if (disc) {
    tomviz::deserialize(disc, json);
  }

  auto* omapProxy =
    vtkSMPropertyHelper(cmap, "ScalarOpacityFunction").GetAsProxy();
  vtkPiecewiseFunction* pwf =
    omapProxy ? vtkPiecewiseFunction::SafeDownCast(
                  omapProxy->GetClientSideObject())
              : nullptr;
  if (pwf) {
    tomviz::deserialize(pwf, json);
  }

  // Record the client-side edits on the proxy properties so proxy-
  // level consumers (state files, rescales) see the same state. Recorded
  // rather than pushed: the VTK objects already hold these points, and
  // a push replays AddPoint per point, quadratic in their number. Bulk
  // buffers for both CTF and PWF (per-element Set doesn't reliably
  // survive UpdateVTKObjects on the opacity sub-proxy).
  if (disc && disc->GetSize() > 0) {
    recordProxyValues(cmap, "RGBPoints", disc->GetDataPointer(),
                      static_cast<unsigned int>(disc->GetSize() * 4));
  }
  cmap->UpdateVTKObjects();
  if (omapProxy && pwf && pwf->GetSize() > 0) {
    const int n = pwf->GetSize();
    std::vector<double> buffer(4 * n);
    for (int i = 0; i < n; ++i) {
      pwf->GetNodeValue(i, buffer.data() + 4 * i);
    }
    recordProxyValues(omapProxy, "Points", buffer.data(),
                      static_cast<unsigned int>(buffer.size()));
    omapProxy->UpdateVTKObjects();
  }
}

} // namespace

LegacyModuleSink::LegacyModuleSink(QObject* parent) : SinkNode(parent) {}

LegacyModuleSink::~LegacyModuleSink()
{
  finalize();
}

QIcon LegacyModuleSink::icon() const
{
  return QIcon(QStringLiteral(":/icons/pqInspect.png"));
}

QIcon LegacyModuleSink::actionIcon() const
{
  if (m_visible) {
    return QIcon(QStringLiteral(":/pqWidgets/Icons/pqEyeball.svg"));
  }
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqEyeballClosed.svg"));
}

void LegacyModuleSink::triggerAction()
{
  setVisibility(!m_visible);
}

bool LegacyModuleSink::initialize(vtkSMViewProxy* view)
{
  if (!view) {
    return false;
  }
  m_viewProxy = view;
  m_renderView =
    vtkPVRenderView::SafeDownCast(view->GetClientSideView());

  // Rendering is wired externally (MainWindow connects renderNeeded() →
  // pqView::render(), which coalesces multiple requests via an internal timer).
  // m_renderView may be null for chart views (e.g. PlotSink) — that's OK,
  // only features requiring a 3D render view will be skipped.

  return true;
}

bool LegacyModuleSink::finalize()
{
  if (m_viewProxy) {
    emit renderNeeded();
  }
  m_renderView = nullptr;
  m_viewProxy = nullptr;
  // The detached CTF (if the user enabled "Separate Color Map") is
  // registered in the session proxy manager by name. Drop its registration
  // so it doesn't outlive this sink across pipeline reset cycles.
  if (m_detachedColorMap) {
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(m_detachedColorMap);
    m_detachedColorMap = nullptr;
  }
  return true;
}

vtkSMViewProxy* LegacyModuleSink::view() const
{
  return m_viewProxy;
}

vtkPVRenderView* LegacyModuleSink::renderView() const
{
  return m_renderView;
}

bool LegacyModuleSink::visibility() const
{
  return m_visible;
}

void LegacyModuleSink::setVisibility(bool visible)
{
  if (m_visible != visible) {
    m_visible = visible;
    emit visibilityChanged(visible);
    emit renderNeeded();
  }
}

bool LegacyModuleSink::isColorMapNeeded() const
{
  return false;
}

bool LegacyModuleSink::useDetachedColorMap() const
{
  return m_useDetachedColorMap;
}

void LegacyModuleSink::setUseDetachedColorMap(bool detached)
{
  if (m_useDetachedColorMap == detached) {
    return;
  }
  m_useDetachedColorMap = detached;

  if (detached && !m_detachedColorMap) {
    // Lazily create the detached color map
    auto* mgr = vtkSMProxyManager::GetProxyManager();
    auto* pxm = mgr ? mgr->GetActiveSessionProxyManager() : nullptr;
    if (!pxm) {
      m_useDetachedColorMap = false;
      return;
    }

    static unsigned int sinkCmapCounter = 0;
    ++sinkCmapCounter;

    vtkNew<vtkSMTransferFunctionManager> tfmgr;
    m_detachedColorMap = tfmgr->GetColorTransferFunction(
      QString("SinkColorMap%1").arg(sinkCmapCounter).toLatin1().data(), pxm);

    // Copy range from VolumeData's color map
    auto vol = m_volumeData.lock();
    if (vol) {
      auto range = vol->scalarRange();
      double r[2] = { range[0], range[1] };
      vtkSMTransferFunctionProxy::RescaleTransferFunction(
        m_detachedColorMap, r);
      auto* detachedOmap =
        vtkSMPropertyHelper(m_detachedColorMap, "ScalarOpacityFunction")
          .GetAsProxy();
      if (detachedOmap) {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(detachedOmap, r);
      }
    }
  }

  updateColorMap();
  emit colorMapChanged();
}

vtkSMProxy* LegacyModuleSink::colorMap()
{
  if (m_useDetachedColorMap && m_detachedColorMap) {
    return m_detachedColorMap;
  }
  auto vol = m_volumeData.lock();
  if (vol && vol->hasColorMap()) {
    return vol->colorMap();
  }
  return nullptr;
}

vtkSMProxy* LegacyModuleSink::opacityMap()
{
  auto* cmap = colorMap();
  if (!cmap) {
    return nullptr;
  }
  return vtkSMPropertyHelper(cmap, "ScalarOpacityFunction").GetAsProxy();
}

vtkPiecewiseFunction* LegacyModuleSink::gradientOpacity() const
{
  if (m_useDetachedColorMap) {
    return m_detachedGradientOpacity;
  }
  auto vol = m_volumeData.lock();
  if (vol) {
    return vol->gradientOpacity();
  }
  return nullptr;
}

VolumeDataPtr LegacyModuleSink::volumeData() const
{
  return m_volumeData.lock();
}

void LegacyModuleSink::updateColorMap()
{
  // Base implementation does nothing. Subclasses override to push
  // color/opacity into their VTK pipeline.
}

void LegacyModuleSink::onMetadataChanged() {}

void LegacyModuleSink::addClippingPlane(vtkPlane*)
{
}

void LegacyModuleSink::removeClippingPlane(vtkPlane*)
{
}

void LegacyModuleSink::clearVisualization()
{
}

void LegacyModuleSink::resetVisualization()
{
  clearVisualization();
  emit renderNeeded();
}

void LegacyModuleSink::restoreVisualization()
{
  // Re-apply the current visibility flag. Each subclass's setVisibility()
  // override pushes visibility onto its props/widgets unconditionally before
  // the base call, so this re-shows props that clearVisualization() hid even
  // though m_visible never changed (the base call's guard then suppresses a
  // redundant visibilityChanged signal). The VTK objects still hold the last
  // consumed data, so nothing needs to be re-read or re-executed.
  setVisibility(m_visible);
  emit renderNeeded();
}

void LegacyModuleSink::onInputDisconnected(InputPort*)
{
  resetVisualization();
}

QList<LegacyModuleSink*> LegacyModuleSink::siblingSinks(
  const QString& inputPortName) const
{
  QList<LegacyModuleSink*> result;
  auto* input = inputPort(inputPortName);
  if (!input || !input->link()) {
    return result;
  }
  auto* upstream = input->link()->from();
  for (auto* link : upstream->links()) {
    auto* sink = qobject_cast<LegacyModuleSink*>(link->to()->node());
    if (sink && sink != this) {
      result.append(sink);
    }
  }
  return result;
}

QJsonObject LegacyModuleSink::serialize() const
{
  QJsonObject json = Node::serialize();
  json["visible"] = m_visible;
  if (m_viewProxy) {
    // Persist the ParaView proxy GlobalID so the loader can re-bind
    // this sink to the same view after restoreViewsAndLayouts() has
    // rebuilt the scene.
    json["viewId"] = static_cast<int>(m_viewProxy->GetGlobalID());
  }
  if (m_useDetachedColorMap) {
    json["useDetachedColorMap"] = true;
    if (m_detachedColorMap) {
      json["colorOpacityMap"] = tomviz::serialize(m_detachedColorMap);
    }
    if (m_detachedGradientOpacity->GetSize() > 0) {
      json["gradientOpacityMap"] =
        tomviz::serialize(m_detachedGradientOpacity.Get());
    }
  }
  return json;
}

bool LegacyModuleSink::deserialize(const QJsonObject& json)
{
  Node::deserialize(json);
  // setUseDetachedColorMap must run before the map payload deserializes
  // so that m_detachedColorMap exists for the proxy-based apply.
  if (json.contains("useDetachedColorMap")) {
    setUseDetachedColorMap(json["useDetachedColorMap"].toBool());
  }
  if (m_useDetachedColorMap) {
    if (json.contains("colorOpacityMap") && m_detachedColorMap) {
      applyColorMapJson(m_detachedColorMap,
                        json.value("colorOpacityMap").toObject());
    }
    if (json.contains("gradientOpacityMap")) {
      tomviz::deserialize(m_detachedGradientOpacity.Get(),
                          json.value("gradientOpacityMap").toObject());
    }
  }
  if (json.contains("visible")) {
    setVisibility(json["visible"].toBool());
  }
  return true;
}

namespace {
constexpr const char* kDefaultScalarsName = "tomviz::DefaultScalars";
} // namespace

QString LegacyModuleSink::activeScalarsToName(int activeScalarsIdx) const
{
  if (activeScalarsIdx < 0) {
    return QString::fromLatin1(kDefaultScalarsName);
  }
  auto vol = m_volumeData.lock();
  if (vol && vol->isValid()) {
    if (auto* pd = vol->imageData()->GetPointData()) {
      if (activeScalarsIdx < pd->GetNumberOfArrays()) {
        if (auto* arr = pd->GetArray(activeScalarsIdx)) {
          if (arr->GetName()) {
            return QString::fromUtf8(arr->GetName());
          }
        }
      }
    }
  }
  return QString::fromLatin1(kDefaultScalarsName);
}

void LegacyModuleSink::readActiveScalars(const QJsonObject& json,
                                          int& activeScalarsIdx)
{
  auto val = json.value("activeScalars");
  if (val.isString()) {
    auto name = val.toString();
    if (name == QLatin1String(kDefaultScalarsName) || name.isEmpty()) {
      activeScalarsIdx = -1;
      m_pendingActiveScalarsName.clear();
      return;
    }
    // Defer resolution until the sink has consumed data; if it's
    // already consumed, resolve now.
    activeScalarsIdx = -1;
    m_pendingActiveScalarsName = name;
    resolvePendingActiveScalar(activeScalarsIdx);
  } else if (!val.isUndefined() && !val.isNull()) {
    activeScalarsIdx = val.toInt(-1);
    m_pendingActiveScalarsName.clear();
  }
}

void LegacyModuleSink::resolvePendingActiveScalar(int& activeScalarsIdx)
{
  if (m_pendingActiveScalarsName.isEmpty()) {
    return;
  }
  auto vol = m_volumeData.lock();
  if (!vol || !vol->isValid()) {
    return;
  }
  auto* pd = vol->imageData()->GetPointData();
  if (!pd) {
    return;
  }
  for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
    auto* arr = pd->GetArray(i);
    if (arr && arr->GetName() &&
        m_pendingActiveScalarsName == QString::fromUtf8(arr->GetName())) {
      activeScalarsIdx = i;
      m_pendingActiveScalarsName.clear();
      return;
    }
  }
}

void LegacyModuleSink::prepareConsume(
  const QMap<QString, PortData>& inputs)
{
  for (auto* port : inputPorts()) {
    if (!acceptsVolumeType(port->acceptedTypes())) {
      continue;
    }
    auto it = inputs.constFind(port->name());
    if (it == inputs.constEnd() || !isVolumeType(it.value().type())) {
      continue;
    }
    auto vol = it.value().value<VolumeDataPtr>();
    if (!vol) {
      continue;
    }
    m_volumeData = vol;
    if (!m_metadataConnected) {
      if (auto* lnk = port->link()) {
        if (auto* outPort = lnk->from()) {
          connect(outPort, &OutputPort::metadataChanged, this,
                  &LegacyModuleSink::onMetadataChanged);
          m_metadataConnected = true;
        }
      }
    }
    break;
  }
}

bool LegacyModuleSink::execute()
{
  bool success = SinkNode::execute();

  if (success && m_firstConsume) {
    m_firstConsume = false;
    resetCameraIfFirstSink();
  }

  return success;
}

void LegacyModuleSink::postConsume(bool success)
{
  if (!success) {
    return;
  }
  // updateColorMap() emits renderNeeded(); if the sink doesn't need
  // a color map, trigger a render directly.
  if (isColorMapNeeded()) {
    updateColorMap();
  } else {
    emit renderNeeded();
  }
}

void LegacyModuleSink::resetCameraIfFirstSink()
{
  if (!m_viewProxy) {
    return;
  }

  // Check if any other sink sharing our view has already consumed data
  auto* pip = qobject_cast<Pipeline*>(parent());
  if (pip) {
    for (auto* node : pip->nodes()) {
      auto* other = dynamic_cast<LegacyModuleSink*>(node);
      if (other && other != this && other->view() == m_viewProxy &&
          !other->m_firstConsume) {
        // Another sink already rendered to this view — skip reset
        return;
      }
    }
  }

  // Schedule camera reset on the main thread. Capture the view as a
  // weak pointer, not raw: m_viewProxy is itself weak because the sink
  // doesn't own the view (it's torn down by pqDeleteReaction::deleteAll()
  // on reset / state reload). This queued call runs on `this` (the
  // sink), which can outlive the view, so a raw capture could fire
  // ResetCamera() on a freed proxy.
  vtkWeakPointer<vtkSMRenderViewProxy> renderViewProxy(
    vtkSMRenderViewProxy::SafeDownCast(m_viewProxy));
  if (renderViewProxy) {
    QMetaObject::invokeMethod(this, [renderViewProxy]() {
      if (renderViewProxy) {
        renderViewProxy->ResetCamera();
      }
    }, Qt::QueuedConnection);
  }
}

QWidget* LegacyModuleSink::createSinkPropertiesWidget(QWidget* /*parent*/)
{
  return nullptr;
}

bool LegacyModuleSink::validateInput(const QMap<QString, PortData>& inputs,
                                     const QString& portName) const
{
  return inputs.contains(portName) && inputs[portName].isValid();
}

} // namespace pipeline
} // namespace tomviz
