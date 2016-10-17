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

#include "DataSource.h"
#include "Utilities.h"

#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkImageShiftScale.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSmartPointer.h>
#include <vtkSmartVolumeMapper.h>
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

ModuleVolume::ModuleVolume(QObject* parentObject) : Superclass(parentObject)
{
}

ModuleVolume::~ModuleVolume()
{
  this->finalize();
}

QIcon ModuleVolume::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqVolumeData16.png");
}

bool ModuleVolume::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkImageShiftScale> t;

  vtkTrivialProducer* trv =
    vtkTrivialProducer::SafeDownCast(data->producer()->GetClientSideObject());
  vtkImageData* im = vtkImageData::SafeDownCast(trv->GetOutputDataObject(0));

  t->SetInputData(im);

  m_volumeMapper->SetInputConnection(t->GetOutputPort());
  m_volume->SetMapper(m_volumeMapper.Get());
  m_volume->SetProperty(m_volumeProperty.Get());

  this->updateColorMap();

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_view->AddPropToRenderer(m_volume.Get());

  return true;
}

void ModuleVolume::updateColorMap()
{
  m_volumeProperty->SetScalarOpacity(
    vtkPiecewiseFunction::SafeDownCast(opacityMap()->GetClientSideObject()));
  m_volumeProperty->SetColor(
    vtkColorTransferFunction::SafeDownCast(colorMap()->GetClientSideObject()));

  // BUG: volume mappers don't update property when LUT is changed and has an
  // older Mtime. Fix for now by forcing the LUT to update.
  vtkObject::SafeDownCast(this->colorMap()->GetClientSideObject())->Modified();
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

bool ModuleVolume::serialize(pugi::xml_node&) const
{
  return false;
}

bool ModuleVolume::deserialize(const pugi::xml_node& ns)
{
  return this->Superclass::deserialize(ns);
}

void ModuleVolume::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;
  QCheckBox* lighting = new QCheckBox("Enable lighting");
  layout->addWidget(lighting);
  panel->setLayout(layout);
  QCheckBox* maxIntensity = new QCheckBox("Maximum intensity");
  layout->addWidget(maxIntensity);
  layout->addStretch();

  lighting->setChecked(m_volumeProperty->GetShade() == 1);
  maxIntensity->setChecked(m_volumeMapper->GetBlendMode() ==
                           vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);

  connect(lighting, SIGNAL(clicked(bool)), SLOT(setLighting(bool)));
  connect(maxIntensity, SIGNAL(clicked(bool)), SLOT(setMaximumIntensity(bool)));
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

void ModuleVolume::setLighting(bool val)
{
  m_volumeProperty->SetShade(val ? 1 : 0);
  m_volumeProperty->SetAmbient(0.0);
  m_volumeProperty->SetDiffuse(1.0);
  m_volumeProperty->SetSpecularPower(100.0);
  emit renderNeeded();
}

void ModuleVolume::setMaximumIntensity(bool val)
{
  if (val) {
    m_volumeMapper->SetBlendMode(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);
  } else {
    m_volumeMapper->SetBlendMode(vtkVolumeMapper::COMPOSITE_BLEND);
  }
  emit renderNeeded();
}

} // end of namespace tomviz
