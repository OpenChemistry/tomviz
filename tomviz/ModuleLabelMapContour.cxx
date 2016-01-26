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
#include "ModuleLabelMapContour.h"

#include "DataSource.h"

#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

#include "pqProxiesWidget.h"

#include <vector>

namespace tomviz
{

//-----------------------------------------------------------------------------
ModuleLabelMapContour::ModuleLabelMapContour(QObject* parentObject) : Superclass(parentObject)
{
  this->ResampleFilter = nullptr;
  this->LabelMapContourRepresentation = nullptr;
}

//-----------------------------------------------------------------------------
ModuleLabelMapContour::~ModuleLabelMapContour()
{
}

//-----------------------------------------------------------------------------
QString ModuleLabelMapContour::label() const
{
  return  "Label Map Contour";
}

//-----------------------------------------------------------------------------
QIcon ModuleLabelMapContour::icon() const
{
  // TODO - change ot custom label map icon
  return QIcon(":/pqWidgets/Icons/pqIsosurface24.png");
}

//-----------------------------------------------------------------------------
bool ModuleLabelMapContour::initialize(DataSource* data,
                                       vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(data, view))
  {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = data->producer()->GetSessionProxyManager();

  // Flying edges used in ModuleContour does not interpolate scalar
  // arrays.  To do that, we use the Probe filter in ParaView to
  // resample label map values onto the contour.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "Probe"));

  this->ResampleFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->ResampleFilter);
  controller->PreInitializeProxy(this->ResampleFilter);
  vtkSMPropertyHelper(this->ResampleFilter, "Input").Set(data->producer());
  vtkSMPropertyHelper(this->ResampleFilter, "Source").Set(this->ContourFilter);
  controller->PostInitializeProxy(this->ResampleFilter);
  controller->RegisterPipelineProxy(this->ResampleFilter);

  controller->Hide(this->ContourRepresentation, view);
  this->LabelMapContourRepresentation = controller->Show(this->ResampleFilter, 0, view);
  Q_ASSERT(this->LabelMapContourRepresentation);

  // TODO - check that the LabelMap array is available
  vtkSMPropertyHelper colorArrayHelper(this->LabelMapContourRepresentation, "ColorArrayName");
  colorArrayHelper.SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS, "LabelMap");

  this->ResampleFilter->UpdateVTKObjects();

  // use proper color map.
  this->updateColorMap();

  this->LabelMapContourRepresentation->UpdateVTKObjects();

  return true;
}

//-----------------------------------------------------------------------------
bool ModuleLabelMapContour::finalize()
{
  this->Superclass::finalize();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ResampleFilter);

  this->ResampleFilter = nullptr;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleLabelMapContour::serialize(pugi::xml_node& ns) const
{
  // TODO - implement
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleLabelMapContour::deserialize(const pugi::xml_node& ns)
{
  // TODO - implement
  return true;
}

//-----------------------------------------------------------------------------
void ModuleLabelMapContour::updateColorMap()
{
  if (!this->LabelMapContourRepresentation)
  {
    return;
  }
  vtkSMPropertyHelper(this->LabelMapContourRepresentation,
                      "LookupTable").Set(this->colorMap());
  this->LabelMapContourRepresentation->UpdateVTKObjects();  
}

//-----------------------------------------------------------------------------
void ModuleLabelMapContour::addToPanel(pqProxiesWidget* panel)
{
  Q_ASSERT(this->ContourFilter);
  Q_ASSERT(this->ResampleFilter);
  Q_ASSERT(this->LabelMapContourRepresentation);

  QStringList contourProperties;
  contourProperties << "ContourValues";
  panel->addProxy(this->ContourFilter, "Contour", contourProperties, true);

  QStringList contourRepresentationProperties;
  contourRepresentationProperties
    << "Representation"
    << "Opacity"
    << "Specular";
  panel->addProxy(this->LabelMapContourRepresentation, "Appearance", contourRepresentationProperties, true);

  // For color map
  this->Module::addToPanel(panel);
}

//-----------------------------------------------------------------------------
bool ModuleLabelMapContour::setVisibility(bool val)
{
  Q_ASSERT(this->LabelMapContourRepresentation);
  vtkSMPropertyHelper(this->LabelMapContourRepresentation, "Visibility").Set(val? 1 : 0);
  this->LabelMapContourRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleLabelMapContour::visibility() const
{
  Q_ASSERT(this->LabelMapContourRepresentation);
  return vtkSMPropertyHelper(this->LabelMapContourRepresentation, "Visibility").GetAsInt() != 0;
}

} // end namespace tomviz
