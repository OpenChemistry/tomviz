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
#include "HistogramManager.h"
#include "vtkTransferFunctionBoxItem.h"

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

ModuleVolume::ModuleVolume(QObject* parentObject) : Module(parentObject)
{
  connect(&HistogramManager::instance(), &HistogramManager::histogram2DReady,
          this, [=](vtkSmartPointer<vtkImageData> image,
                    vtkSmartPointer<vtkImageData> histogram2D) {
            // Force the transfer function 2D to update.
            if (image ==
                vtkImageData::SafeDownCast(dataSource()->dataObject())) {
              auto colorMap = vtkColorTransferFunction::SafeDownCast(
                this->colorMap()->GetClientSideObject());
              auto opacityMap = vtkPiecewiseFunction::SafeDownCast(
                this->opacityMap()->GetClientSideObject());
              vtkTransferFunctionBoxItem::rasterTransferFunction2DBox(
                histogram2D, *this->transferFunction2DBox(),
                transferFunction2D(), colorMap, opacityMap);
            }
            // Update the volume mapper.
            this->updateColorMap();
            emit this->renderNeeded();
          });
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
  vtkTrivialProducer* trv = data->producer();
  m_volumeMapper->SetInputConnection(trv->GetOutputPort());
  m_volume->SetMapper(m_volumeMapper.Get());
  m_volume->SetProperty(m_volumeProperty.Get());
  const double* displayPosition = data->displayPosition();
  m_volume->SetPosition(displayPosition[0], displayPosition[1],
                        displayPosition[2]);
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
      if (transferFunction2D() && transferFunction2D()->GetExtent()[1] > 0) {
        propertyMode = vtkVolumeProperty::TF_2D;
        m_volumeProperty->SetTransferFunction2D(transferFunction2D());
      } else {
        vtkSmartPointer<vtkImageData> image =
          vtkImageData::SafeDownCast(dataSource()->dataObject());
        // See if the histogram is done, if it is then update the transfer
        // function.
        if (auto histogram2D =
              HistogramManager::instance().getHistogram2D(image)) {
          auto colorMap = vtkColorTransferFunction::SafeDownCast(
            this->colorMap()->GetClientSideObject());
          auto opacityMap = vtkPiecewiseFunction::SafeDownCast(
            this->opacityMap()->GetClientSideObject());
          vtkTransferFunctionBoxItem::rasterTransferFunction2DBox(
            histogram2D, *this->transferFunction2DBox(), transferFunction2D(),
            colorMap, opacityMap);
          propertyMode = vtkVolumeProperty::TF_2D;
          m_volumeProperty->SetTransferFunction2D(transferFunction2D());
        }
        // If this histogram is not ready, then it finishing will trigger the
        // functor created in the constructor and the volume mapper will be
        // updated when the histogram2D is done
      }
      break;
  }

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

QJsonObject ModuleVolume::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["transferMode"] = getTransferMode();
  props["interpolation"] = m_volumeProperty->GetInterpolationType();
  props["blendingMode"] = m_volumeMapper->GetBlendMode();
  props["rayJittering"] = m_volumeMapper->GetUseJittering() == 1;

  QJsonObject lighting;
  lighting["enabled"] = m_volumeProperty->GetShade() == 1;
  lighting["ambient"] = m_volumeProperty->GetAmbient();
  lighting["diffuse"] = m_volumeProperty->GetDiffuse();
  lighting["specular"] = m_volumeProperty->GetSpecular();
  lighting["specularPower"] = m_volumeProperty->GetSpecularPower();
  props["lighting"] = lighting;

  json["properties"] = props;
  return json;
}

bool ModuleVolume::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();

    setTransferMode(
      static_cast<Module::TransferMode>(props["transferMode"].toInt()));
    onInterpolationChanged(props["interpolation"].toInt());
    setBlendingMode(props["blendingMode"].toInt());
    setJittering(props["rayJittering"].toBool());

    if (props["lighting"].isObject()) {
      auto lighting = props["lighting"].toObject();
      setLighting(lighting["enabled"].toBool());
      onAmbientChanged(lighting["ambient"].toDouble());
      onDiffuseChanged(lighting["diffuse"].toDouble());
      onSpecularChanged(lighting["specular"].toDouble());
      onSpecularPowerChanged(lighting["specularPower"].toDouble());
    }

    updatePanel();
    return true;
  }
  return false;
}

void ModuleVolume::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }
  if (!m_controllers) {
    m_controllers = new ModuleVolumeWidget;
  }

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  layout->addWidget(m_controllers);
  updatePanel();

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

void ModuleVolume::updatePanel()
{
  // If m_controllers is present update the values, if not they will be updated
  // when it is created and shown.
  if (!m_controllers || !m_volumeMapper || !m_volumeProperty) {
    return;
  }
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
  vtkTrivialProducer* trv = this->dataSource()->producer();
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
