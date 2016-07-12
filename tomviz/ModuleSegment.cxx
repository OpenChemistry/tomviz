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
#include "Utilities.h"
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
#include <QHBoxLayout>

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
      "    # An example segmentation script follows: \n\n"
      "    # Create a filter (ConfidenceConnectedImageFilter) for the input image type\n"
      "    itk_filter = itk.ConfidenceConnectedImageFilter[itk_image_type,itk.Image.SS3].New()\n\n"
      "    # Set input parameters on the filter (these are copied from an example in ITK.\n"
      "    itk_filter.SetInitialNeighborhoodRadius(3)\n"
      "    itk_filter.SetMultiplier(3)\n"
      "    itk_filter.SetNumberOfIterations(25)\n"
      "    itk_filter.SetReplaceValue(255)\n"
      "    itk_filter.SetSeed((24,65,37))\n\n"
      "    # Hand the input image to the filter\n"
      "    itk_filter.SetInput(itk_image)\n"
      "    # Run the filter\n"
      "    itk_filter.Update()\n\n"
      "    # Return the output and the output type (itk.Image.SS3 is one of the valid output\n"
      "    # types for this filter and is the one we specified when we created the filter above\n"
      "    return itk_filter.GetOutput(), itk.Image.SS3\n"
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
  vtkSMPropertyHelper(this->Internals->ContourRepresentation, "Position").Set(data->displayPosition(), 3);

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
  this->Internals->ProgrammableFilter = nullptr;
  this->Internals->ContourFilter = nullptr;
  this->Internals->ContourRepresentation = nullptr;
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
  pugi::xml_node node = ns.append_child("ITKScript");
  QStringList scriptProps;
  scriptProps << "Script";
  if (!tomviz::serialize(this->Internals->SegmentationScript, node, scriptProps))
  {
    qWarning("Failed to serialize script.");
    ns.remove_child(node);
    return false;
  }

  node = ns.append_child("ContourFilter");
  QStringList contourProps;
  contourProps << "ContourValues";
  if (!tomviz::serialize(this->Internals->ContourFilter, node, contourProps))
  {
    qWarning("Failed to serialize contour.");
    ns.remove_child(node);
    return false;
  }

  node = ns.append_child("ContourRepresentation");
  QStringList representationProps;
  representationProps << "Representation" << "Opacity" << "Specular" << "Visibility";
  if (!tomviz::serialize(this->Internals->ContourRepresentation, node, representationProps))
  {
    qWarning("Failed to serialize ContourRepresentation");
    ns.remove_child(node);
    return false;
  }
  return this->Superclass::serialize(ns);
}

bool ModuleSegment::deserialize(const pugi::xml_node &ns)
{
  return tomviz::deserialize(this->Internals->SegmentationScript, ns.child("ITKScript")) &&
    tomviz::deserialize(this->Internals->ContourFilter, ns.child("ContourFilter")) &&
    tomviz::deserialize(this->Internals->ContourRepresentation, ns.child("ContourRepresentation")) &&
    this->Superclass::deserialize(ns);
}

void ModuleSegment::addToPanel(QWidget *panel)
{
  Q_ASSERT(this->Internals->ProgrammableFilter);

  if (panel->layout()) {
    delete panel->layout();
  }

  QHBoxLayout *layout = new QHBoxLayout;
  panel->setLayout(layout);
  pqProxiesWidget *proxiesWidget = new pqProxiesWidget(panel);
  layout->addWidget(proxiesWidget);

  QStringList properties;
  properties << "Script";
  proxiesWidget->addProxy(this->Internals->SegmentationScript, "Script", properties, true);

  Q_ASSERT(this->Internals->ContourFilter);
  Q_ASSERT(this->Internals->ContourRepresentation);

  QStringList contourProperties;
  contourProperties << "ContourValues";
  proxiesWidget->addProxy(this->Internals->ContourFilter, "Contour", contourProperties, true);

  QStringList contourRepresentationProperties;
  contourRepresentationProperties
    << "Representation"
    << "Opacity"
    << "Specular";
  proxiesWidget->addProxy(this->Internals->ContourRepresentation, "Appearance", contourRepresentationProperties, true);
  proxiesWidget->updateLayout();
  this->connect(proxiesWidget, SIGNAL(changeFinished(vtkSMProxy*)),
                SIGNAL(renderNeeded()));
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

void ModuleSegment::updateColorMap()
{
  Q_ASSERT(this->Internals->ContourRepresentation);
  vtkSMPropertyHelper(this->Internals->ContourRepresentation,
                      "LookupTable").Set(this->colorMap());
  this->Internals->ContourRepresentation->UpdateVTKObjects();
}

void ModuleSegment::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = {newX, newY, newZ};
  vtkSMPropertyHelper(this->Internals->ContourRepresentation, "Position").
    Set(pos, 3);
}

//-----------------------------------------------------------------------------
bool ModuleSegment::isProxyPartOfModule(vtkSMProxy *proxy)
{
  return (proxy == this->Internals->ProgrammableFilter.Get()) ||
         (proxy == this->Internals->ContourFilter.Get()) ||
         (proxy == this->Internals->ContourRepresentation.Get());
}

std::string ModuleSegment::getStringForProxy(vtkSMProxy *proxy)
{
  if (proxy == this->Internals->ProgrammableFilter.Get())
  {
    return "ProgrammableFilter";
  }
  else if (proxy == this->Internals->ContourFilter.Get())
  {
    return "Contour";
  }
  else if (proxy == this->Internals->ContourRepresentation.Get())
  {
    return "Representation";
  }
  else
  {
    qWarning("Unknown proxy passed to module segment in save animation");
    return "";
  }
}

vtkSMProxy *ModuleSegment::getProxyForString(const std::string& str)
{
  if (str == "ProgrammableFilter")
  {
    return this->Internals->ProgrammableFilter.Get();
  }
  else if (str == "ContourFilter")
  {
    return this->Internals->ContourFilter.Get();
  }
  else if (str == "Representation")
  {
    return this->Internals->ContourRepresentation.Get();
  }
  else
  {
    qWarning("Unknown proxy passed to module segment in save animation");
    return nullptr;
  }
}

}
