/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "dax/ModuleStreamingContour.h"

#include "DataSource.h"
#include "Utilities.h"

#include "pqProxiesWidget.h"
#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

#include <vector>
#include <algorithm>

namespace TEM
{

//-----------------------------------------------------------------------------
ModuleStreamingContour::ModuleStreamingContour(QObject* parentObject) :Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleStreamingContour::~ModuleStreamingContour()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleStreamingContour::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqIsosurface24.png");
}

//-----------------------------------------------------------------------------
bool ModuleStreamingContour::initialize(DataSource* dataSource, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(dataSource, view))
    {
    return false;
    }

  vtkSMSourceProxy* producer = dataSource->producer();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  // Create the representation for it.
  this->ContourRepresentation = controller->Show(producer, 0, view);
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Representation").Set("Streaming Contour");


  // ColorArrayName
  const char* propertyName = producer->GetDataInformation()->
                                       GetPointDataInformation()->
                                       GetArrayInformation(0)->
                                       GetName();

  vtkSMPVRepresentationProxy::SetScalarColoring(this->ContourRepresentation,
                                                propertyName,
                                                vtkDataObject::POINT );
  this->ContourRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleStreamingContour::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ContourRepresentation);
  this->ContourRepresentation = NULL;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleStreamingContour::setVisibility(bool val)
{
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Visibility").Set(val? 1 : 0);
  this->ContourRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleStreamingContour::visibility() const
{
  Q_ASSERT(this->ContourRepresentation);
  return vtkSMPropertyHelper(this->ContourRepresentation, "Visibility").GetAsInt() != 0;
}

//-----------------------------------------------------------------------------
void ModuleStreamingContour::setIsoValues(const QList<double>& values)
{
  double vectorValue(1);
  if(values.size() > 0)
    {
    vectorValue = values[0];
    }

  vtkSMPropertyHelper(this->ContourRepresentation,"ContourValue").Set(
                                                                vectorValue);
  this->ContourRepresentation->UpdateVTKObjects();
}


//-----------------------------------------------------------------------------
void ModuleStreamingContour::addToPanel(pqProxiesWidget* panel)
{
  Q_ASSERT(this->ContourRepresentation);

  QStringList contourProperties;
  contourProperties << "ContourValue";
  panel->addProxy(this->ContourRepresentation, "Contour", contourProperties, true);

  QStringList contourRepresentationProperties;
  contourRepresentationProperties
    << "Color"
    << "ColorEditor"
    << "Representation"
    << "Opacity"
    << "Specular";
  panel->addProxy(this->ContourRepresentation, "Appearance", contourRepresentationProperties, true);
}

//-----------------------------------------------------------------------------
bool ModuleStreamingContour::serialize(pugi::xml_node& ns) const
{
  QStringList contourProperties;
  contourProperties << "ContourValue"
    << "Color"
    << "ColorEditor"
    << "Representation"
    << "Opacity"
    << "Specular"
    << "Visibility";
  pugi::xml_node node = ns.append_child("ContourRepresentation");

  return TEM::serialize(this->ContourRepresentation, node, contourProperties);
}

//-----------------------------------------------------------------------------
bool ModuleStreamingContour::deserialize(const pugi::xml_node& ns)
{
  return TEM::deserialize(this->ContourRepresentation, ns.child("ContourRepresentation"));
}

} // end of namespace TEM
