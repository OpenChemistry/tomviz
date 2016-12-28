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
#include <vtkCommand.h>
#include <vtkNew.h>
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
  vtkSmartPointer<vtkSMProxy> DetachedColorMap;
  vtkSmartPointer<vtkSMProxy> DetachedOpacityMap;

public:
  vtkWeakPointer<vtkSMProxy> ColorMap;
  vtkWeakPointer<vtkSMProxy> OpacityMap;

  vtkSMProxy* detachedColorMap()
  {
    if (!this->DetachedColorMap) {
      static unsigned int colorMapCounter = 0;
      colorMapCounter++;

      vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();

      vtkNew<vtkSMTransferFunctionManager> tfmgr;
      this->DetachedColorMap = tfmgr->GetColorTransferFunction(
        QString("ModuleColorMap%1").arg(colorMapCounter).toLatin1().data(),
        pxm);
      this->DetachedOpacityMap =
        vtkSMPropertyHelper(this->DetachedColorMap, "ScalarOpacityFunction")
          .GetAsProxy();
    }
    return this->DetachedColorMap;
  }

  vtkSMProxy* detachedOpacityMap()
  {
    this->detachedColorMap();
    return this->DetachedOpacityMap;
  }
};

Module::Module(QObject* parentObject)
  : QObject(parentObject), Internals(new Module::MInternals())
{
}

Module::~Module()
{
}

bool Module::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  m_view = vtkView;
  m_activeDataSource = data;
  if (m_view && m_activeDataSource) {
    // FIXME: we're connecting this too many times. Fix it.
    tomviz::convert<pqView*>(vtkView)->connect(
      m_activeDataSource, SIGNAL(dataChanged()), SLOT(render()));
    this->connect(m_activeDataSource,
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

void Module::setUseDetachedColorMap(bool val)
{
  m_useDetachedColorMap = val;
  if (this->isColorMapNeeded() == false) {
    return;
  }

  if (m_useDetachedColorMap) {
    this->Internals->ColorMap = this->Internals->detachedColorMap();
    this->Internals->OpacityMap = this->Internals->detachedOpacityMap();

    tomviz::rescaleColorMap(this->Internals->ColorMap, this->dataSource());
    pqCoreUtilities::connect(this->Internals->ColorMap,
                             vtkCommand::ModifiedEvent, this,
                             SLOT(onColorMapChanged()));
  } else {
    this->Internals->ColorMap = nullptr;
    this->Internals->OpacityMap = nullptr;
  }
  this->updateColorMap();
  emit colorMapChanged();
}

vtkSMProxy* Module::colorMap() const
{
  return this->useDetachedColorMap() ? this->Internals->ColorMap.GetPointer()
                                     : this->dataSource()->colorMap();
}

vtkSMProxy* Module::opacityMap() const
{
  Q_ASSERT(this->Internals->ColorMap || !m_useDetachedColorMap);
  return this->useDetachedColorMap() ? this->Internals->OpacityMap.GetPointer()
                                     : this->dataSource()->opacityMap();
}

bool Module::serialize(pugi::xml_node& ns) const
{
  if (this->isColorMapNeeded()) {
    ns.append_attribute("use_detached_colormap")
      .set_value(m_useDetachedColorMap ? 1 : 0);
    if (m_useDetachedColorMap) {
      pugi::xml_node nodeL = ns.append_child("ColorMap");
      pugi::xml_node nodeS = ns.append_child("OpacityMap");

      // Using detached color map, so we need to save the local color map.
      if (tomviz::serialize(this->colorMap(), nodeL) == false ||
          tomviz::serialize(this->opacityMap(), nodeS) == false) {
        return false;
      }
    }
  }
  return true;
}

bool Module::deserialize(const pugi::xml_node& ns)
{
  if (this->isColorMapNeeded()) {
    bool dcm = ns.attribute("use_detached_colormap").as_int(0) == 1;
    if (dcm && ns.child("ColorMap")) {
      if (!tomviz::deserialize(this->Internals->detachedColorMap(),
                               ns.child("ColorMap"))) {
        qCritical("Failed to deserialze ColorMap");
        return false;
      }
    }
    if (dcm && ns.child("OpacityMap")) {
      if (!tomviz::deserialize(this->Internals->detachedOpacityMap(),
                               ns.child("OpacityMap"))) {
        qCritical("Failed to deserialze OpacityMap");
        return false;
      }
    }
    this->setUseDetachedColorMap(dcm);
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

} // end of namespace tomviz
