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
#include "ModuleThreshold.h"

#include "DataSource.h"
#include "pqProxiesWidget.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

namespace TEM
{

//-----------------------------------------------------------------------------
ModuleThreshold::ModuleThreshold(QObject* parentObject): Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleThreshold::~ModuleThreshold()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleThreshold::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqThreshold24.png");
}

//-----------------------------------------------------------------------------
bool ModuleThreshold::initialize(DataSource* dataSource, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(dataSource, view))
    {
    return false;
    }

  vtkSMSourceProxy* producer = dataSource->producer();

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
  double mid = ((range[0] + range[1])/2.0);
  newRange[0] = mid - 0.001 * delta;
  newRange[1] = mid + 0.001 * delta;
  rangeProperty.Set(newRange, 2);
  this->ThresholdFilter->UpdateVTKObjects();

  // Create the representation for it.
  this->ThresholdRepresentation = controller->Show(this->ThresholdFilter, 0, view);
  Q_ASSERT(this->ThresholdRepresentation);
  vtkSMPropertyHelper(this->ThresholdRepresentation, "Representation").Set("Surface");
  this->ThresholdRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleThreshold::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ThresholdRepresentation);
  controller->UnRegisterProxy(this->ThresholdFilter);
  this->ThresholdFilter = NULL;
  this->ThresholdRepresentation = NULL;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleThreshold::setVisibility(bool val)
{
  Q_ASSERT(this->ThresholdRepresentation);
  vtkSMPropertyHelper(this->ThresholdRepresentation, "Visibility").Set(val? 1 : 0);
  this->ThresholdRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleThreshold::visibility() const
{
  Q_ASSERT(this->ThresholdRepresentation);
  return vtkSMPropertyHelper(this->ThresholdRepresentation, "Visibility").GetAsInt() != 0;
}

//-----------------------------------------------------------------------------
void ModuleThreshold::addToPanel(pqProxiesWidget* panel)
{
  Q_ASSERT(this->ThresholdFilter);
  Q_ASSERT(this->ThresholdRepresentation);

  QStringList fprops;
  fprops << "SelectInputScalars" << "ThresholdBetween";

  panel->addProxy(this->ThresholdFilter, "Threshold", fprops, true);

  QStringList representationProperties;
  representationProperties
    << "Representation"
    << "Opacity"
    << "Specular";
  panel->addProxy(this->ThresholdRepresentation, "Appearance", representationProperties, true);

  this->Superclass::addToPanel(panel);
}


}
