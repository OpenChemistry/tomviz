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
#include "Module.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "Utilities.h"

#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqCoreUtilities.h>
#include <pqPVApplicationCore.h>
#include <pqProxiesWidget.h>
#include <pqView.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>

namespace tomviz {

class Module::MInternals
{
  vtkSmartPointer<vtkSMProxy> m_detachedColorMap;
  vtkSmartPointer<vtkSMProxy> m_detachedOpacityMap;

public:
  vtkWeakPointer<vtkSMProxy> m_colorMap;
  vtkWeakPointer<vtkSMProxy> m_opacityMap;
  vtkNew<vtkPiecewiseFunction> m_gradientOpacityMap;

  Module::TransferMode m_transferMode;
  vtkNew<vtkImageData> m_transfer2D;
  vtkSMProxy* detachedColorMap()
  {
    if (!m_detachedColorMap) {
      static unsigned int colorMapCounter = 0;
      ++colorMapCounter;

      auto pxm = ActiveObjects::instance().proxyManager();

      vtkNew<vtkSMTransferFunctionManager> tfmgr;
      m_detachedColorMap = tfmgr->GetColorTransferFunction(
        QString("ModuleColorMap%1").arg(colorMapCounter).toLatin1().data(),
        pxm);
      m_detachedOpacityMap =
        vtkSMPropertyHelper(m_detachedColorMap, "ScalarOpacityFunction")
          .GetAsProxy();
    }
    return m_detachedColorMap;
  }

  vtkSMProxy* detachedOpacityMap()
  {
    detachedColorMap();
    return m_detachedOpacityMap;
  }
};

Module::Module(QObject* parentObject)
  : QObject(parentObject), d(new Module::MInternals())
{
}

Module::~Module() = default;

bool Module::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  m_view = vtkView;
  m_activeDataSource = data;
  d->m_gradientOpacityMap->RemoveAllPoints();
  d->m_transfer2D->SetDimensions(1, 1, 1);
  d->m_transfer2D->AllocateScalars(VTK_FLOAT, 4);

  if (m_view && m_activeDataSource) {
    // FIXME: we're connecting this too many times. Fix it.
    tomviz::convert<pqView*>(vtkView)->connect(
      m_activeDataSource, SIGNAL(dataChanged()), SLOT(render()));
    connect(m_activeDataSource, SIGNAL(dataChanged()), this,
            SIGNAL(dataSourceChanged()));
    connect(m_activeDataSource,
            SIGNAL(displayPositionChanged(double, double, double)),
            SLOT(dataSourceMoved(double, double, double)));
  }
  return (m_view && m_activeDataSource);
}

vtkSMViewProxy* Module::view() const
{
  return m_view;
}

DataSource* Module::dataSource() const
{
  return m_activeDataSource;
}

void Module::addToPanel(QWidget* vtkNotUsed(panel))
{
}

void Module::prepareToRemoveFromPanel(QWidget* vtkNotUsed(panel))
{
}

void Module::setUseDetachedColorMap(bool val)
{
  m_useDetachedColorMap = val;
  if (isColorMapNeeded() == false) {
    return;
  }

  if (m_useDetachedColorMap) {
    d->m_colorMap = d->detachedColorMap();
    d->m_opacityMap = d->detachedOpacityMap();

    tomviz::rescaleColorMap(d->m_colorMap, dataSource());
    pqCoreUtilities::connect(d->m_colorMap, vtkCommand::ModifiedEvent, this,
                             SLOT(onColorMapChanged()));
  } else {
    d->m_colorMap = nullptr;
    d->m_opacityMap = nullptr;
  }
  updateColorMap();
  emit colorMapChanged();
}

vtkSMProxy* Module::colorMap() const
{
  return useDetachedColorMap() ? d->m_colorMap.GetPointer()
                               : colorMapDataSource()->colorMap();
}

vtkSMProxy* Module::opacityMap() const
{
  Q_ASSERT(d->m_colorMap || !m_useDetachedColorMap);
  return useDetachedColorMap() ? d->m_opacityMap.GetPointer()
                               : dataSource()->opacityMap();
}

vtkPiecewiseFunction* Module::gradientOpacityMap() const
{
  vtkPiecewiseFunction* gof = useDetachedColorMap()
                                ? d->m_gradientOpacityMap.GetPointer()
                                : dataSource()->gradientOpacityMap();

  // Set default values
  const int numPoints = gof->GetSize();
  if (numPoints == 0) {
    vtkColorTransferFunction* lut =
      vtkColorTransferFunction::SafeDownCast(colorMap()->GetClientSideObject());

    double range[2];
    lut->GetRange(range);

    // For gradient magnitude, the volume mapper's fragment shader expects a
    // range of [0, DataMax/4].
    const double maxValue = (range[1] - range[0]) / 4.0;
    gof->AddPoint(0.0, 0.0);
    gof->AddPoint(maxValue, 1.0);
  }

  return gof;
}

vtkImageData* Module::transferFunction2D() const
{
  return useDetachedColorMap() ? d->m_transfer2D.GetPointer()
                               : colorMapDataSource()->transferFunction2D();
}

bool Module::serialize(pugi::xml_node& ns) const
{
  if (isColorMapNeeded()) {
    ns.append_attribute("use_detached_colormap")
      .set_value(m_useDetachedColorMap ? 1 : 0);
    if (m_useDetachedColorMap) {
      pugi::xml_node nodeL = ns.append_child("ColorMap");
      pugi::xml_node nodeS = ns.append_child("OpacityMap");

      // Using detached color map, so we need to save the local color map.
      if (tomviz::serialize(colorMap(), nodeL) == false ||
          tomviz::serialize(opacityMap(), nodeS) == false) {
        return false;
      }

      pugi::xml_node nodeGrad = ns.append_child("GradientOpacityMap");
      tomviz::serialize(gradientOpacityMap(), nodeGrad);
    }
  }
  return true;
}

bool Module::deserialize(const pugi::xml_node& ns)
{
  if (isColorMapNeeded()) {
    bool dcm = ns.attribute("use_detached_colormap").as_int(0) == 1;
    if (dcm && ns.child("ColorMap")) {
      if (!tomviz::deserialize(d->detachedColorMap(), ns.child("ColorMap"))) {
        qCritical("Failed to deserialze ColorMap");
        return false;
      }
    }
    if (dcm && ns.child("OpacityMap")) {
      if (!tomviz::deserialize(d->detachedOpacityMap(),
                               ns.child("OpacityMap"))) {
        qCritical("Failed to deserialze OpacityMap");
        return false;
      }
    }
    pugi::xml_node nodeGrad = ns.child("GradientOpacityMap");
    if (dcm && nodeGrad) {
      tomviz::deserialize(d->m_gradientOpacityMap.GetPointer(), nodeGrad);
    }
    setUseDetachedColorMap(dcm);
  }
  return true;
}

void Module::onColorMapChanged()
{
  emit colorMapChanged();
}

bool Module::serializeAnimationCue(pqAnimationCue* cue, const char* proxyName,
                                   pugi::xml_node& ns, const char* helperName)
{
  vtkSMProperty* property = cue->getAnimatedProperty();
  int propertyIndex = cue->getAnimatedPropertyIndex();
  const char* propName = property ? property->GetXMLName() : "";

  pugi::xml_node n = ns.append_child("cue");
  n.append_attribute("proxy").set_value(proxyName);
  n.append_attribute("property").set_value(propName);
  n.append_attribute("propertyIndex").set_value(propertyIndex);
  if (helperName) {
    n.append_attribute("onHelper").set_value(helperName);
  }
  QList<vtkSMProxy*> keyframes = cue->getKeyFrames();
  for (int i = 0; i < keyframes.size(); ++i) {
    pugi::xml_node keyframeNode = n.append_child("keyframe");
    tomviz::serialize(keyframes[i], keyframeNode);
  }
  return true;
}

bool Module::serializeAnimationCue(pqAnimationCue* cue, Module* module,
                                   pugi::xml_node& ns, const char* helperName,
                                   vtkSMProxy* realProxy)
{
  vtkSMProxy* animated = cue->getAnimatedProxy();
  std::string proxy =
    module->getStringForProxy(realProxy ? realProxy : animated);
  return serializeAnimationCue(cue, proxy.c_str(), ns, helperName);
}

bool Module::deserializeAnimationCue(Module* module, const pugi::xml_node& ns)
{
  const pugi::xml_node& cueNode = ns.child("cue");
  std::string proxy = cueNode.attribute("proxy").value();
  vtkSMProxy* proxyObj = module->getProxyForString(proxy);
  return deserializeAnimationCue(proxyObj, ns);
}

bool Module::deserializeAnimationCue(vtkSMProxy* proxyObj,
                                     const pugi::xml_node& ns)
{
  if (proxyObj == nullptr) {
    return false;
  }
  const pugi::xml_node& cueNode = ns.child("cue");
  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();
  const char* property = cueNode.attribute("property").value();
  int propertyIndex = cueNode.attribute("propertyIndex").as_int();
  if (cueNode.attribute("onHelper")) {
    const char* helperKey = cueNode.attribute("onHelper").value();
    QList<vtkSMProxy*> helpers =
      tomviz::convert<pqProxy*>(proxyObj)->getHelperProxies(helperKey);
    if (helpers.size() == 1) {
      proxyObj = helpers[0];
    } else {
      qWarning("Not possible to tell which helper proxy needed to load cue");
      return false;
    }
  }

  const char* type;
  if (vtkSMRenderViewProxy::SafeDownCast(proxyObj)) {
    type = "CameraAnimationCue";
  } else {
    type = "KeyFrameAnimationCue";
  }

  pqAnimationCue* cue =
    scene->createCue(proxyObj, property, propertyIndex, type);
  int i = 0;
  for (pugi::xml_node keyframeNode = cueNode.child("keyframe"); keyframeNode;
       keyframeNode = keyframeNode.next_sibling("keyframe")) {
    vtkSMProxy* keyframe = nullptr;
    if (i < cue->getNumberOfKeyFrames()) {
      keyframe = cue->getKeyFrame(i);
    } else {
      keyframe = cue->insertKeyFrame(i);
    }
    if (!tomviz::deserialize(keyframe, keyframeNode)) {
      scene->removeCue(cue);
      return false;
    }
    keyframe->UpdateVTKObjects();
    ++i;
  }
  cue->triggerKeyFramesModified();
  return true;
}

void Module::setTransferMode(const TransferMode mode)
{
  d->m_transferMode = static_cast<Module::TransferMode>(mode);
  this->updateColorMap();

  emit transferModeChanged(static_cast<int>(mode));
}

Module::TransferMode Module::getTransferMode() const
{
  return d->m_transferMode;
}

vtkSmartPointer<vtkDataObject> Module::getDataToExport()
{
  return nullptr;
}

} // end of namespace tomviz
