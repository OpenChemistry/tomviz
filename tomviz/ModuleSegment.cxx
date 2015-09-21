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

#include "ModuleSegment.h"

#include "DataSource.h"
#include "pqCoreUtilities.h"
#include "pqProxiesWidget.h"
#include "vtkAlgorithm.h"
#include "vtkCommand.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"

#include <QIcon>

namespace tomviz
{

class ModuleSegment::MSInternal
{
public:
  vtkSmartPointer<vtkSMProxy> SegmentationScript;
  vtkSmartPointer<vtkSMSourceProxy> ProgrammableFilter;
  vtkSmartPointer<vtkSMSourceProxy> ContourFilter;
  vtkSmartPointer<vtkSMProxy> ContourRepresentation;
  bool IsVisible;
};

ModuleSegment::ModuleSegment(QObject* p)
  : Superclass(p), Internals(new MSInternal)
{
}

ModuleSegment::~ModuleSegment()
{
  this->finalize();
}

QString ModuleSegment::label() const
{
  return "Segmentation";
}

QIcon ModuleSegment::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqCalculator24.png");
}

bool ModuleSegment::initialize(DataSource *data, vtkSMViewProxy *vtkView)
{
  if (!this->Superclass::initialize(data, vtkView))
  {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMSourceProxy* producer = data->producer();
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  this->Internals->SegmentationScript.TakeReference(
      pxm->NewProxy("tomviz_proxies", "PythonProgrammableSegmentation"));
  vtkSMPropertyHelper(this->Internals->SegmentationScript, "Script").Set(
      "def run_itk_segmentation(itk_image, itk_image_type):\n"
      "    # should return the result image and result image type like this:\n"
      "    # return outImage, outImageType\n"
      "    return itk_image, itk_image_type\n"
      );

  vtkSmartPointer<vtkSMProxy> proxy;

  proxy.TakeReference(pxm->NewProxy("filters", "ProgrammableFilter"));
  this->Internals->ProgrammableFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->Internals->ProgrammableFilter);

  pqCoreUtilities::connect(
    this->Internals->SegmentationScript, vtkCommand::PropertyModifiedEvent,
    this, SLOT(onPropertyChanged()));

  controller->PreInitializeProxy(this->Internals->ProgrammableFilter);
  vtkSMPropertyHelper(this->Internals->ProgrammableFilter, "Input").Set(producer);
  vtkSMPropertyHelper(this->Internals->ProgrammableFilter, "OutputDataSetType").Set(/*vtkImageData*/6);
  vtkSMPropertyHelper(this->Internals->ProgrammableFilter, "Script").Set("self.GetOutput().ShallowCopy(self.GetInput())\n");
  controller->PostInitializeProxy(this->Internals->ProgrammableFilter);
  controller->RegisterPipelineProxy(this->Internals->ProgrammableFilter);

  proxy.TakeReference(pxm->NewProxy("filters", "Contour"));
  this->Internals->ContourFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->Internals->ContourFilter);

  controller->PreInitializeProxy(this->Internals->ContourFilter);
  vtkSMPropertyHelper(this->Internals->ContourFilter, "Input").Set(this->Internals->ProgrammableFilter);
  vtkSMPropertyHelper(this->Internals->ContourFilter, "ComputeScalars", /*quiet*/ true).Set(1);

  controller->PostInitializeProxy(this->Internals->ContourFilter);
  controller->RegisterPipelineProxy(this->Internals->ContourFilter);

  vtkAlgorithm *alg = vtkAlgorithm::SafeDownCast(this->Internals->ContourFilter->GetClientSideObject());
  alg->SetInputArrayToProcess(0, 0, 0, 0, "ImageScalars");

  this->Internals->ContourRepresentation = controller->Show(this->Internals->ContourFilter, 0, vtkView);
  Q_ASSERT(this->Internals->ContourRepresentation);
  vtkSMPropertyHelper(this->Internals->ContourRepresentation, "Representation").Set("Surface");

  updateColorMap();

  this->Internals->ProgrammableFilter->UpdateVTKObjects();
  this->Internals->ContourFilter->UpdateVTKObjects();
  this->Internals->ContourRepresentation->UpdateVTKObjects();

  return true;
}

bool ModuleSegment::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->Internals->ProgrammableFilter);
  controller->UnRegisterProxy(this->Internals->ContourRepresentation);
  controller->UnRegisterProxy(this->Internals->ContourFilter);
  this->Internals->ProgrammableFilter = NULL;
  this->Internals->ContourFilter = NULL;
  this->Internals->ContourRepresentation = NULL;
  return true;
}

bool ModuleSegment::visibility() const
{
  Q_ASSERT(this->Internals->ContourRepresentation);
  return vtkSMPropertyHelper(this->Internals->ContourRepresentation, "Visibility").GetAsInt() != 0;
}

bool ModuleSegment::setVisibility(bool val)
{
  Q_ASSERT(this->Internals->ContourRepresentation);
  vtkSMPropertyHelper(this->Internals->ContourRepresentation, "Visibility").Set(val? 1 : 0);
  this->Internals->ContourRepresentation->UpdateVTKObjects();
  return true;
}

bool ModuleSegment::serialize(pugi::xml_node &ns) const
{
  return false; // TODO
}

bool ModuleSegment::deserialize(const pugi::xml_node &ns)
{
  return false; // TODO
}

void ModuleSegment::addToPanel(pqProxiesWidget *panel)
{
  Q_ASSERT(this->Internals->ProgrammableFilter);
  QStringList properties;
  properties << "Script";
  panel->addProxy(this->Internals->SegmentationScript, "Script", properties, true);

  Q_ASSERT(this->Internals->ContourFilter);
  Q_ASSERT(this->Internals->ContourRepresentation);

  QStringList contourProperties;
  contourProperties << "ContourValues";
  panel->addProxy(this->Internals->ContourFilter, "Contour", contourProperties, true);

  QStringList contourRepresentationProperties;
  contourRepresentationProperties
    << "Representation"
    << "Opacity"
    << "Specular";
  panel->addProxy(this->Internals->ContourRepresentation, "Appearance", contourRepresentationProperties, true);


  this->Superclass::addToPanel(panel);
}

void ModuleSegment::onPropertyChanged()
{
  std::cout << "Got property changed..." << std::endl;
  QString userScript = 
      vtkSMPropertyHelper(this->Internals->SegmentationScript, "Script").GetAsString();
  QString segmentScript = QString(
    "import vtk\n"
    "from tomviz import utils\n"
    "import itk\n"
    "\n"
    "idi = self.GetInput()\n"
    "ido = self.GetOutput()\n"
    "ido.DeepCopy(idi)\n"
    "\n"
    "array = utils.get_array(idi)\n"
    "itk_image_type = itk.Image.F3\n"
    "itk_converter = itk.PyBuffer[itk_image_type]\n"
    "itk_image = itk_converter.GetImageFromArray(array)\n"
    "\n"
    "%1\n"
    "\n"
    "output_itk_image, output_type = run_itk_segmentation(itk_image, itk_image_type)\n"
    "\n"
    "output_array = itk.PyBuffer[output_type].GetArrayFromImage(output_itk_image)\n"
    "utils.set_array(ido, output_array)\n"
    ""
    "if array.shape == output_array.shape:\n"
    "    ido.SetOrigin(idi.GetOrigin())\n"
    "    ido.SetExtent(idi.GetExtent())\n"
    "    ido.SetSpacing(idi.GetSpacing())\n").arg(userScript);
  vtkSMPropertyHelper(this->Internals->ProgrammableFilter, "Script").Set(
      segmentScript.toLatin1().data());
  this->Internals->ProgrammableFilter->UpdateVTKObjects();
  // TODO
}

//-----------------------------------------------------------------------------
void ModuleSegment::updateColorMap()
{
  Q_ASSERT(this->Internals->ContourRepresentation);
  vtkSMPropertyHelper(this->Internals->ContourRepresentation,
                      "LookupTable").Set(this->colorMap());
  this->Internals->ContourRepresentation->UpdateVTKObjects();
}
}
