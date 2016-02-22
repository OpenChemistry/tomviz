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
#include "ModuleContour.h"

#include "DataSource.h"
#include "pqProxiesWidget.h"
#include "Utilities.h"

#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

#include <algorithm>
#include <string>
#include <vector>

namespace tomviz
{

//-----------------------------------------------------------------------------
class ModuleContour::Private
{
public:
  std::string NonLabelMapArrayName;
};

//-----------------------------------------------------------------------------
ModuleContour::ModuleContour(QObject* parentObject) : Superclass(parentObject)
{
  this->Internals = new Private;
}

//-----------------------------------------------------------------------------
ModuleContour::~ModuleContour()
{
  this->finalize();

  delete this->Internals;
  this->Internals = nullptr;
}

//-----------------------------------------------------------------------------
QIcon ModuleContour::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqIsosurface24.png");
}

//-----------------------------------------------------------------------------
bool ModuleContour::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView))
  {
    return false;
  }

  vtkSMSourceProxy* producer = data->producer();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  vtkSmartPointer<vtkSMProxy> contourProxy;
  contourProxy.TakeReference(pxm->NewProxy("filters", "FlyingEdges"));

  this->ContourFilter = vtkSMSourceProxy::SafeDownCast(contourProxy);
  Q_ASSERT(this->ContourFilter);
  controller->PreInitializeProxy(this->ContourFilter);
  vtkSMPropertyHelper(this->ContourFilter, "Input").Set(producer);
  vtkSMPropertyHelper(this->ContourFilter, "ComputeScalars", /*quiet*/ true).Set(1);

  controller->PostInitializeProxy(this->ContourFilter);
  controller->RegisterPipelineProxy(this->ContourFilter);

  // Set up a data resampler to add LabelMap values on the contour
  vtkSmartPointer<vtkSMProxy> probeProxy;
  probeProxy.TakeReference(pxm->NewProxy("filters", "Probe"));

  this->ResampleFilter = vtkSMSourceProxy::SafeDownCast(probeProxy);
  Q_ASSERT(this->ResampleFilter);
  controller->PreInitializeProxy(this->ResampleFilter);
  vtkSMPropertyHelper(this->ResampleFilter, "Input").Set(data->producer());
  vtkSMPropertyHelper(this->ResampleFilter, "Source").Set(this->ContourFilter);
  controller->PostInitializeProxy(this->ResampleFilter);
  controller->RegisterPipelineProxy(this->ResampleFilter);

  // Create the representation for it. Show the unresampled contour filter to start.
  this->ContourRepresentation = controller->Show(this->ContourFilter, 0, vtkView);
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Representation").Set("Surface");
  vtkSMPropertyHelper(this->ContourRepresentation, "Position").Set(data->displayPosition(), 3);

  vtkSMPropertyHelper colorArrayHelper(this->ContourRepresentation, "ColorArrayName");
  this->Internals->NonLabelMapArrayName = std::string(colorArrayHelper.GetInputArrayNameToProcess());

  // use proper color map.
  this->updateColorMap();

  this->ContourRepresentation->UpdateVTKObjects();

  return true;
}

//-----------------------------------------------------------------------------
void ModuleContour::updateColorMap()
{
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation,
                      "LookupTable").Set(this->colorMap());
  vtkSMPropertyHelper colorArrayHelper(this->ContourRepresentation, "ColorArrayName");

  if (this->colorByLabelMap())
  {
    this->Internals->NonLabelMapArrayName = std::string(colorArrayHelper.GetInputArrayNameToProcess());
    colorArrayHelper.SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS, "LabelMap");

    vtkSMPropertyHelper(this->ContourRepresentation, "Input").Set(this->ResampleFilter);
  }
  else
  {
    colorArrayHelper.SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS, this->Internals->NonLabelMapArrayName.c_str());
    vtkSMPropertyHelper(this->ContourRepresentation, "Input").Set(this->ContourFilter);
  }

  vtkSMPropertyHelper(this->ContourRepresentation, "Visibility").Set(this->visibility() ? 1 : 0);
  this->ContourRepresentation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleContour::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ResampleFilter);
  controller->UnRegisterProxy(this->ContourRepresentation);
  controller->UnRegisterProxy(this->ContourFilter);
  this->ResampleFilter = nullptr;
  this->ContourFilter = nullptr;
  this->ContourRepresentation = nullptr;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleContour::setVisibility(bool val)
{
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Visibility").Set(val? 1 : 0);
  this->ContourRepresentation->UpdateVTKObjects();

  return true;
}

//-----------------------------------------------------------------------------
bool ModuleContour::visibility() const
{
  Q_ASSERT(this->ContourRepresentation);
  return vtkSMPropertyHelper(this->ContourRepresentation, "Visibility").GetAsInt() != 0;
}

//-----------------------------------------------------------------------------
void ModuleContour::setIsoValues(const QList<double>& values)
{
  std::vector<double> vectorValues(values.size());
  std::copy(values.begin(), values.end(), vectorValues.begin());
  vectorValues.push_back(0); // to avoid having to check for 0 size on Windows.

  vtkSMPropertyHelper(this->ContourFilter,"ContourValues").Set(
    &vectorValues[0], values.size());
  this->ContourFilter->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
void ModuleContour::addToPanel(pqProxiesWidget* panel)
{
  Q_ASSERT(this->ContourFilter);
  Q_ASSERT(this->ContourRepresentation);

  QStringList contourProperties;
  contourProperties << "ContourValues";
  panel->addProxy(this->ContourFilter, "Contour", contourProperties, true);

  QStringList contourRepresentationProperties;
  contourRepresentationProperties
    << "Representation"
    << "Opacity"
    << "Specular";
  panel->addProxy(this->ContourRepresentation, "Appearance", contourRepresentationProperties, true);

  this->Superclass::addToPanel(panel);
}

//-----------------------------------------------------------------------------
bool ModuleContour::serialize(pugi::xml_node& ns) const
{
  // save stuff that the user can change.
  pugi::xml_node node = ns.append_child("ContourFilter");
  QStringList contourProperties;
  contourProperties << "ContourValues";
  if (tomviz::serialize(this->ContourFilter, node, contourProperties) == false)
  {
    qWarning("Failed to serialize ContourFilter.");
    ns.remove_child(node);
    return false;
  }

  QStringList contourRepresentationProperties;
  contourRepresentationProperties
    << "Representation"
    << "Opacity"
    << "Specular"
    << "Visibility";

  node = ns.append_child("ContourRepresentation");
  if (tomviz::serialize(this->ContourRepresentation, node, contourRepresentationProperties) == false)
  {
    qWarning("Failed to serialize ContourRepresentation.");
    ns.remove_child(node);
    return false;
  }

  return this->Superclass::serialize(ns);
}

//-----------------------------------------------------------------------------
bool ModuleContour::deserialize(const pugi::xml_node& ns)
{
  return tomviz::deserialize(this->ContourFilter, ns.child("ContourFilter")) &&
    tomviz::deserialize(this->ContourRepresentation, ns.child("ContourRepresentation")) &&
    this->Superclass::deserialize(ns);
}

//-----------------------------------------------------------------------------
void ModuleContour::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = {newX, newY, newZ};
  vtkSMPropertyHelper(this->ContourRepresentation, "Position").Set(pos, 3);
  this->ContourRepresentation->MarkDirty(this->ContourRepresentation);
  this->ContourRepresentation->UpdateVTKObjects();
}

} // end of namespace tomviz
