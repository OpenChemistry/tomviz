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
#include "ModuleThreshold.h"

#include "DataSource.h"
#include "pqProxiesWidget.h"
#include "Utilities.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

#include <QHBoxLayout>
namespace tomviz
{

ModuleThreshold::ModuleThreshold(QObject* parentObject)
  : Superclass(parentObject)
{
}

ModuleThreshold::~ModuleThreshold()
{
  this->finalize();
}

QIcon ModuleThreshold::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqThreshold24.png");
}

bool ModuleThreshold::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView))
  {
    return false;
  }

  vtkSMSourceProxy* producer = data->producer();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  // Create the contour filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "Threshold"));

  this->ThresholdFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->ThresholdFilter);
  controller->PreInitializeProxy(this->ThresholdFilter);
  vtkSMPropertyHelper(this->ThresholdFilter, "Input").Set(producer);
  controller->PostInitializeProxy(this->ThresholdFilter);
  controller->RegisterPipelineProxy(this->ThresholdFilter);

  // Update min/max to avoid thresholding the full dataset.
  vtkSMPropertyHelper rangeProperty(this->ThresholdFilter, "ThresholdBetween");
  double range[2], newRange[2];
  rangeProperty.Get(range, 2);
  double delta = (range[1] - range[0]);
  double mid = ((range[0] + range[1]) / 2.0);
  newRange[0] = mid - 0.1 * delta;
  newRange[1] = mid + 0.1 * delta;
  rangeProperty.Set(newRange, 2);
  this->ThresholdFilter->UpdateVTKObjects();

  // Create the representation for it.
  this->ThresholdRepresentation = controller->Show(this->ThresholdFilter, 0,
                                                   vtkView);
  Q_ASSERT(this->ThresholdRepresentation);
  vtkSMRepresentationProxy::SetRepresentationType(this->ThresholdRepresentation,
                                                  "Surface");
  vtkSMPropertyHelper(this->ThresholdRepresentation, "Position").Set(data->displayPosition(), 3);
  this->updateColorMap();
  this->ThresholdRepresentation->UpdateVTKObjects();
  return true;
}

void ModuleThreshold::updateColorMap()
{
  Q_ASSERT(this->ThresholdRepresentation);

  // by default, use the data source's color/opacity maps.
  vtkSMPropertyHelper(this->ThresholdRepresentation,
                      "LookupTable").Set(this->colorMap());
  vtkSMPropertyHelper(this->ThresholdRepresentation,
                      "ScalarOpacityFunction").Set(this->opacityMap());

  this->ThresholdRepresentation->UpdateVTKObjects();
}

bool ModuleThreshold::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ThresholdRepresentation);
  controller->UnRegisterProxy(this->ThresholdFilter);
  this->ThresholdFilter = nullptr;
  this->ThresholdRepresentation = nullptr;
  return true;
}

bool ModuleThreshold::setVisibility(bool val)
{
  Q_ASSERT(this->ThresholdRepresentation);
  vtkSMPropertyHelper(this->ThresholdRepresentation,
                      "Visibility").Set(val? 1 : 0);
  this->ThresholdRepresentation->UpdateVTKObjects();
  return true;
}

bool ModuleThreshold::visibility() const
{
  Q_ASSERT(this->ThresholdRepresentation);
  return vtkSMPropertyHelper(this->ThresholdRepresentation,
                             "Visibility").GetAsInt() != 0;
}

void ModuleThreshold::addToPanel(QWidget* panel)
{
  Q_ASSERT(this->ThresholdFilter);
  Q_ASSERT(this->ThresholdRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QHBoxLayout *layout = new QHBoxLayout;
  panel->setLayout(layout);
  pqProxiesWidget *proxiesWidget = new pqProxiesWidget(panel);
  layout->addWidget(proxiesWidget);

  QStringList fprops;
  fprops << "SelectInputScalars" << "ThresholdBetween";

  proxiesWidget->addProxy(this->ThresholdFilter, "Threshold", fprops, true);

  QStringList representationProperties;
  representationProperties
    << "Representation"
    << "Opacity"
    << "Specular";
  proxiesWidget->addProxy(this->ThresholdRepresentation, "Appearance", representationProperties, true);
  proxiesWidget->updateLayout();
}

bool ModuleThreshold::serialize(pugi::xml_node& ns) const
{
  QStringList fprops;
  fprops << "SelectInputScalars" << "ThresholdBetween";
  pugi::xml_node tnode = ns.append_child("Threshold");

  QStringList representationProperties;
  representationProperties
    << "Representation"
    << "Opacity"
    << "Specular"
    << "Visibility";
  pugi::xml_node rnode = ns.append_child("ThresholdRepresentation");
  return tomviz::serialize(this->ThresholdFilter, tnode, fprops) &&
    tomviz::serialize(this->ThresholdRepresentation, rnode, representationProperties) &&
    this->Superclass::serialize(ns);
}

bool ModuleThreshold::deserialize(const pugi::xml_node& ns)
{
  return tomviz::deserialize(this->ThresholdFilter, ns.child("Threshold")) &&
    tomviz::deserialize(this->ThresholdRepresentation,
                     ns.child("ThresholdRepresentation")) &&
    this->Superclass::deserialize(ns);
}

void ModuleThreshold::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = {newX, newY, newZ};
  vtkSMPropertyHelper(this->ThresholdRepresentation, "Position").Set(pos, 3);
  this->ThresholdRepresentation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleThreshold::isProxyPartOfModule(vtkSMProxy *proxy)
{
  return (proxy == this->ThresholdFilter.Get()) ||
         (proxy == this->ThresholdRepresentation.Get());
}

std::string ModuleThreshold::getStringForProxy(vtkSMProxy *proxy)
{
  if (proxy == this->ThresholdFilter.Get())
  {
    return "Threshold";
  }
  else if (proxy == this->ThresholdRepresentation.Get())
  {
    return "Representation";
  }
  else
  {
    qWarning("Unknown proxy passed to module threshold in save animation");
    return "";
  }
}

vtkSMProxy *ModuleThreshold::getProxyForString(const std::string& str)
{
  if (str == "Threshold")
  {
    return this->ThresholdFilter.Get();
  }
  else if (str == "Representation")
  {
    return this->ThresholdRepresentation.Get();
  }
  else
  {
    return nullptr;
  }
}

}
