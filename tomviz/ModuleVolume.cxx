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
#include "ModuleVolume.h"
#include "ModuleVolumeWidget.h"

#include "DataSource.h"
#include "Utilities.h"

#include <vtkColorTransferFunction.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>
#include <vtkVector.h>
#include <vtkView.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <pqProxiesWidget.h>
#include <vtkPVRenderView.h>
#include <vtkSMPVRepresentationProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QVBoxLayout>

namespace tomviz {

using pugi::xml_attribute;
using pugi::xml_node;

ModuleVolume::ModuleVolume(QObject* parentObject) : Module(parentObject)
{
}

ModuleVolume::~ModuleVolume()
{
  finalize();
}

QIcon ModuleVolume::icon() const
{
  return QIcon(":/icons/pqVolumeData.png");
}

bool ModuleVolume::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  // Default parameters
  vtkTrivialProducer* trv = vtkTrivialProducer::SafeDownCast(
    data->dataSourceProxy()->GetClientSideObject());
  m_volumeMapper->SetInputConnection(trv->GetOutputPort());
  m_volume->SetMapper(m_volumeMapper.Get());
  m_volume->SetProperty(m_volumeProperty.Get());
  m_volumeMapper->UseJitteringOn();
  m_volumeMapper->SetBlendMode(vtkVolumeMapper::COMPOSITE_BLEND);
  m_volumeProperty->SetInterpolationType(VTK_LINEAR_INTERPOLATION);
  m_volumeProperty->SetAmbient(0.0);
  m_volumeProperty->SetDiffuse(1.0);
  m_volumeProperty->SetSpecular(1.0);
  m_volumeProperty->SetSpecularPower(100.0);

  updateColorMap();

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_view->AddPropToRenderer(m_volume.Get());
  m_view->Update();

  return true;
}

void ModuleVolume::updateColorMap()
{
  m_volumeProperty->SetScalarOpacity(
    vtkPiecewiseFunction::SafeDownCast(opacityMap()->GetClientSideObject()));
  m_volumeProperty->SetColor(
    vtkColorTransferFunction::SafeDownCast(colorMap()->GetClientSideObject()));

  int propertyMode = vtkVolumeProperty::TF_1D;
  const Module::TransferMode mode = getTransferMode();
  switch (mode) {
    case (Module::SCALAR):
      m_volumeProperty->SetGradientOpacity(nullptr);
      break;
    case (Module::GRADIENT_1D):
      m_volumeProperty->SetGradientOpacity(gradientOpacityMap());
      break;
    case (Module::GRADIENT_2D):
      propertyMode = vtkVolumeProperty::TF_2D;
      break;
  }

  m_volumeProperty->SetTransferFunction2D(transferFunction2D());
  m_volumeProperty->SetTransferFunctionMode(propertyMode);

  // BUG: volume mappers don't update property when LUT is changed and has an
  // older Mtime. Fix for now by forcing the LUT to update.
  vtkObject::SafeDownCast(colorMap()->GetClientSideObject())->Modified();
}

bool ModuleVolume::finalize()
{
  if (m_view) {
    m_view->RemovePropFromRenderer(m_volume.Get());
  }

  return true;
}

bool ModuleVolume::setVisibility(bool val)
{
  m_volume->SetVisibility(val ? 1 : 0);
  return true;
}

bool ModuleVolume::visibility() const
{
  return m_volume->GetVisibility() != 0;
}

bool ModuleVolume::serialize(pugi::xml_node& ns) const
{
  xml_node rootNode = ns.append_child("properties");

  xml_node visibilityNode = rootNode.append_child("visibility");
  visibilityNode.append_attribute("enabled") = visibility();

  xml_node lightingNode = rootNode.append_child("lighting");
  lightingNode.append_attribute("enabled") = m_volumeProperty->GetShade() == 1;
  lightingNode.append_attribute("ambient") = m_volumeProperty->GetAmbient();
  lightingNode.append_attribute("diffuse") = m_volumeProperty->GetDiffuse();
  lightingNode.append_attribute("specular") = m_volumeProperty->GetSpecular();
  lightingNode.append_attribute("specular_power") =
    m_volumeProperty->GetSpecularPower();

  xml_node interpNode = rootNode.append_child("interpolation");
  interpNode.append_attribute("type") =
    m_volumeProperty->GetInterpolationType();

  xml_node gopNode = rootNode.append_child("transfer_function");
  gopNode.append_attribute("mode") = getTransferMode();

  xml_node blendingNode = rootNode.append_child("blending");
  blendingNode.append_attribute("mode") = m_volumeMapper->GetBlendMode();

  xml_node jitteringNode = rootNode.append_child("jittering");
  jitteringNode.append_attribute("enabled") =
    m_volumeMapper->GetUseJittering() == 1;

  return Module::serialize(ns);
}

bool ModuleVolume::deserialize(const pugi::xml_node& ns)
{
  xml_node rootNode = ns.child("properties");
  if (!rootNode) {
    return false;
  }

  xml_node node = rootNode.child("visibility");
  if (node) {
    xml_attribute att = node.attribute("enabled");
    if (att) {
      setVisibility(att.as_bool());
    }
  }
  node = rootNode.child("lighting");
  if (node) {
    xml_attribute att = node.attribute("enabled");
    if (att) {
      setLighting(att.as_bool());
    }
    att = node.attribute("ambient");
    if (att) {
      onAmbientChanged(att.as_double());
    }
    att = node.attribute("diffuse");
    if (att) {
      onDiffuseChanged(att.as_double());
    }
    att = node.attribute("specular");
    if (att) {
      onSpecularChanged(att.as_double());
    }
    att = node.attribute("specular_power");
    if (att) {
      onSpecularPowerChanged(att.as_double());
    }
  }
  node = rootNode.child("blending");
  if (node) {
    xml_attribute att = node.attribute("mode");
    if (att) {
      setBlendingMode(att.as_int());
    }
  }
  node = rootNode.child("interpolation");
  if (node) {
    xml_attribute att = node.attribute("type");
    if (att) {
      onInterpolationChanged(att.as_int());
    }
  }
  node = rootNode.child("jittering");
  if (node) {
    xml_attribute att = node.attribute("enabled");
    if (att) {
      setJittering(att.as_bool());
    }
  }
  node = rootNode.child("transfer_function");
  if (node) {
    xml_attribute att = node.attribute("mode");
    if (att) {
      setTransferMode(static_cast<Module::TransferMode>(att.as_int()));
    }
  }

  return Module::deserialize(ns);
}

void ModuleVolume::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
    m_controllers = nullptr;
  }

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  m_controllers = new ModuleVolumeWidget;
  layout->addWidget(m_controllers);

  m_controllers->setJittering(
    static_cast<bool>(m_volumeMapper->GetUseJittering()));
  m_controllers->setLighting(static_cast<bool>(m_volumeProperty->GetShade()));
  m_controllers->setBlendingMode(m_volumeMapper->GetBlendMode());
  m_controllers->setAmbient(m_volumeProperty->GetAmbient());
  m_controllers->setDiffuse(m_volumeProperty->GetDiffuse());
  m_controllers->setSpecular(m_volumeProperty->GetSpecular());
  m_controllers->setSpecularPower(m_volumeProperty->GetSpecularPower());
  m_controllers->setInterpolationType(m_volumeProperty->GetInterpolationType());

  const auto tfMode = getTransferMode();
  m_controllers->setTransferMode(tfMode);

  connect(m_controllers, SIGNAL(jitteringToggled(const bool)), this,
          SLOT(setJittering(const bool)));
  connect(m_controllers, SIGNAL(lightingToggled(const bool)), this,
          SLOT(setLighting(const bool)));
  connect(m_controllers, SIGNAL(blendingChanged(const int)), this,
          SLOT(setBlendingMode(const int)));
  connect(m_controllers, SIGNAL(interpolationChanged(const int)), this,
          SLOT(onInterpolationChanged(const int)));
  connect(m_controllers, SIGNAL(ambientChanged(const double)), this,
          SLOT(onAmbientChanged(const double)));
  connect(m_controllers, SIGNAL(diffuseChanged(const double)), this,
          SLOT(onDiffuseChanged(const double)));
  connect(m_controllers, SIGNAL(specularChanged(const double)), this,
          SLOT(onSpecularChanged(const double)));
  connect(m_controllers, SIGNAL(specularPowerChanged(const double)), this,
          SLOT(onSpecularPowerChanged(const double)));
  connect(m_controllers, SIGNAL(transferModeChanged(const int)), this,
          SLOT(onTransferModeChanged(const int)));
}

void ModuleVolume::onTransferModeChanged(const int mode)
{
  setTransferMode(static_cast<Module::TransferMode>(mode));
  updateColorMap();

  emit transferModeChanged(mode);
  emit renderNeeded();
}

vtkSmartPointer<vtkDataObject> ModuleVolume::getDataToExport()
{
  vtkTrivialProducer* trv =
    vtkTrivialProducer::SafeDownCast(this->dataSource()->dataSourceProxy());
  return trv->GetOutputDataObject(0);
}

void ModuleVolume::onAmbientChanged(const double value)
{
  m_volumeProperty->SetAmbient(value);
  emit renderNeeded();
}

void ModuleVolume::onDiffuseChanged(const double value)
{
  m_volumeProperty->SetDiffuse(value);
  emit renderNeeded();
}

void ModuleVolume::onSpecularChanged(const double value)
{
  m_volumeProperty->SetSpecular(value);
  emit renderNeeded();
}

void ModuleVolume::onSpecularPowerChanged(const double value)
{
  m_volumeProperty->SetSpecularPower(value);
  emit renderNeeded();
}

void ModuleVolume::onInterpolationChanged(const int type)
{
  m_volumeProperty->SetInterpolationType(type);
  emit renderNeeded();
}

void ModuleVolume::dataSourceMoved(double newX, double newY, double newZ)
{
  vtkVector3d pos(newX, newY, newZ);
  m_volume->SetPosition(pos.GetData());
}

bool ModuleVolume::isProxyPartOfModule(vtkSMProxy*)
{
  return false;
}

std::string ModuleVolume::getStringForProxy(vtkSMProxy*)
{
  qWarning("Unknown proxy passed to module volume in save animation");
  return "";
}

vtkSMProxy* ModuleVolume::getProxyForString(const std::string&)
{
  return nullptr;
}

void ModuleVolume::setLighting(const bool val)
{
  m_volumeProperty->SetShade(val ? 1 : 0);
  emit renderNeeded();
}

void ModuleVolume::setBlendingMode(const int mode)
{
  m_volumeMapper->SetBlendMode(mode);
  emit renderNeeded();
}

void ModuleVolume::setJittering(const bool val)
{
  m_volumeMapper->SetUseJittering(val ? 1 : 0);
  emit renderNeeded();
}

} // end of namespace tomviz
