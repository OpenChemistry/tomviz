/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleSegment.h"

#include "DataSource.h"
#include "Utilities.h"
#include "pqCoreUtilities.h"
#include "pqProxiesWidget.h"
#include "vtkAlgorithm.h"
#include "vtkCommand.h"
#include "vtkNew.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSmartPointer.h"

#include <QHBoxLayout>
#include <QIcon>

namespace tomviz {

class ModuleSegment::MSInternal
{
public:
  vtkSmartPointer<vtkSMProxy> SegmentationScript;
  vtkSmartPointer<vtkSMSourceProxy> ProgrammableFilter;
  vtkSmartPointer<vtkSMSourceProxy> ContourFilter;
  vtkSmartPointer<vtkSMProxy> ContourRepresentation;
  bool IsVisible;
};

ModuleSegment::ModuleSegment(QObject* p) : Module(p), d(new MSInternal) {}

ModuleSegment::~ModuleSegment()
{
  finalize();
}

QString ModuleSegment::label() const
{
  return "Segmentation";
}

QIcon ModuleSegment::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqCalculator24.png");
}

bool ModuleSegment::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMSourceProxy* producer = data->proxy();
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  d->SegmentationScript.TakeReference(
    pxm->NewProxy("tomviz_proxies", "PythonProgrammableSegmentation"));
  vtkSMPropertyHelper(d->SegmentationScript, "Script")
    .Set(
      "def run_itk_segmentation(itk_image, itk_image_type):\n"
      "    # should return the result image and result image type like this:\n"
      "    # return outImage, outImageType\n"
      "    # An example segmentation script follows: \n\n"
      "    # Create a filter (ConfidenceConnectedImageFilter) for the input "
      "image type\n"
      "    itk_filter = "
      "itk.ConfidenceConnectedImageFilter[itk_image_type,itk.Image.SS3].New()"
      "\n\n"
      "    # Set input parameters on the filter (these are copied from an "
      "example in ITK.\n"
      "    itk_filter.SetInitialNeighborhoodRadius(3)\n"
      "    itk_filter.SetMultiplier(3)\n"
      "    itk_filter.SetNumberOfIterations(25)\n"
      "    itk_filter.SetReplaceValue(255)\n"
      "    itk_filter.SetSeed((24,65,37))\n\n"
      "    # Hand the input image to the filter\n"
      "    itk_filter.SetInput(itk_image)\n"
      "    # Run the filter\n"
      "    itk_filter.Update()\n\n"
      "    # Return the output and the output type (itk.Image.SS3 is one of "
      "the valid output\n"
      "    # types for this filter and is the one we specified when we created "
      "the filter above\n"
      "    return itk_filter.GetOutput(), itk.Image.SS3\n");

  vtkSmartPointer<vtkSMProxy> proxy;

  proxy.TakeReference(pxm->NewProxy("filters", "ProgrammableFilter"));
  d->ProgrammableFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(d->ProgrammableFilter);

  pqCoreUtilities::connect(d->SegmentationScript,
                           vtkCommand::PropertyModifiedEvent, this,
                           SLOT(onPropertyChanged()));

  controller->PreInitializeProxy(d->ProgrammableFilter);
  vtkSMPropertyHelper(d->ProgrammableFilter, "Input").Set(producer);
  vtkSMPropertyHelper(d->ProgrammableFilter, "OutputDataSetType")
    .Set(/*vtkImageData*/ 6);
  vtkSMPropertyHelper(d->ProgrammableFilter, "Script")
    .Set("self.GetOutput().ShallowCopy(self.GetInput())\n");
  controller->PostInitializeProxy(d->ProgrammableFilter);
  controller->RegisterPipelineProxy(d->ProgrammableFilter);

  proxy.TakeReference(pxm->NewProxy("filters", "Contour"));
  d->ContourFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(d->ContourFilter);

  controller->PreInitializeProxy(d->ContourFilter);
  vtkSMPropertyHelper(d->ContourFilter, "Input").Set(d->ProgrammableFilter);
  vtkSMPropertyHelper(d->ContourFilter, "ComputeScalars",
                      /*quiet*/ true)
    .Set(1);

  controller->PostInitializeProxy(d->ContourFilter);
  controller->RegisterPipelineProxy(d->ContourFilter);

  vtkAlgorithm* alg =
    vtkAlgorithm::SafeDownCast(d->ContourFilter->GetClientSideObject());
  alg->SetInputArrayToProcess(0, 0, 0, 0, "ImageScalars");

  d->ContourRepresentation = controller->Show(d->ContourFilter, 0, vtkView);
  Q_ASSERT(d->ContourRepresentation);
  vtkSMPropertyHelper(d->ContourRepresentation, "Representation")
    .Set("Surface");
  vtkSMPropertyHelper(d->ContourRepresentation, "Position")
    .Set(data->displayPosition(), 3);

  updateColorMap();

  d->ProgrammableFilter->UpdateVTKObjects();
  d->ContourFilter->UpdateVTKObjects();
  d->ContourRepresentation->UpdateVTKObjects();

  return true;
}

bool ModuleSegment::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(d->ProgrammableFilter);
  controller->UnRegisterProxy(d->ContourRepresentation);
  controller->UnRegisterProxy(d->ContourFilter);
  d->ProgrammableFilter = nullptr;
  d->ContourFilter = nullptr;
  d->ContourRepresentation = nullptr;
  return true;
}

bool ModuleSegment::visibility() const
{
  Q_ASSERT(d->ContourRepresentation);
  return vtkSMPropertyHelper(d->ContourRepresentation, "Visibility")
           .GetAsInt() != 0;
}

bool ModuleSegment::setVisibility(bool val)
{
  Q_ASSERT(d->ContourRepresentation);
  vtkSMPropertyHelper(d->ContourRepresentation, "Visibility").Set(val ? 1 : 0);
  d->ContourRepresentation->UpdateVTKObjects();

  Module::setVisibility(val);

  return true;
}

void ModuleSegment::addToPanel(QWidget* panel)
{
  Q_ASSERT(d->ProgrammableFilter);

  if (panel->layout()) {
    delete panel->layout();
  }

  QHBoxLayout* layout = new QHBoxLayout;
  panel->setLayout(layout);
  pqProxiesWidget* proxiesWidget = new pqProxiesWidget(panel);
  layout->addWidget(proxiesWidget);

  QStringList properties;
  properties << "Script";
  proxiesWidget->addProxy(d->SegmentationScript, "Script", properties, true);

  Q_ASSERT(d->ContourFilter);
  Q_ASSERT(d->ContourRepresentation);

  QStringList contourProperties;
  contourProperties << "ContourValues";
  proxiesWidget->addProxy(d->ContourFilter, "Contour", contourProperties, true);

  QStringList contourRepresentationProperties;
  contourRepresentationProperties << "Representation"
                                  << "Opacity"
                                  << "Specular";
  proxiesWidget->addProxy(d->ContourRepresentation, "Appearance",
                          contourRepresentationProperties, true);
  proxiesWidget->updateLayout();
  connect(proxiesWidget, SIGNAL(changeFinished(vtkSMProxy*)),
          SIGNAL(renderNeeded()));
}

void ModuleSegment::onPropertyChanged()
{
  std::cout << "Got property changed..." << std::endl;
  QString userScript =
    vtkSMPropertyHelper(d->SegmentationScript, "Script").GetAsString();
  QString segmentScript =
    QString("import vtk\n"
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
            "output_itk_image, output_type = run_itk_segmentation(itk_image, "
            "itk_image_type)\n"
            "\n"
            "output_array = "
            "itk.PyBuffer[output_type].GetArrayFromImage(output_itk_image)\n"
            "utils.set_array(ido, output_array)\n"
            ""
            "if array.shape == output_array.shape:\n"
            "    ido.SetOrigin(idi.GetOrigin())\n"
            "    ido.SetExtent(idi.GetExtent())\n"
            "    ido.SetSpacing(idi.GetSpacing())\n")
      .arg(userScript);
  vtkSMPropertyHelper(d->ProgrammableFilter, "Script")
    .Set(segmentScript.toLatin1().data());
  d->ProgrammableFilter->UpdateVTKObjects();
  // TODO
}

void ModuleSegment::updateColorMap()
{
  Q_ASSERT(d->ContourRepresentation);
  vtkSMPropertyHelper(d->ContourRepresentation, "LookupTable").Set(colorMap());
  d->ContourRepresentation->UpdateVTKObjects();
}

void ModuleSegment::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(d->ContourRepresentation, "Position").Set(pos, 3);
}

} // namespace tomviz
