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
#include "DoubleSliderWidget.h"
#include "Utilities.h"

#include "pqColorChooserButton.h"
#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqWidgetRangeDomain.h"

#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"

#include <algorithm>
#include <string>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>

namespace tomviz {

class ModuleContour::Private
{
public:
  std::string NonLabelMapArrayName;
  bool UseSolidColor;
  pqPropertyLinks Links;
};

ModuleContour::ModuleContour(QObject* parentObject) : Superclass(parentObject)
{
  this->Internals = new Private;
  this->Internals->Links.setAutoUpdateVTKObjects(true);
  this->Internals->UseSolidColor = false;
}

ModuleContour::~ModuleContour()
{
  this->finalize();

  delete this->Internals;
  this->Internals = nullptr;
}

QIcon ModuleContour::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqIsosurface24.png");
}

bool ModuleContour::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView)) {
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
  vtkSMPropertyHelper(this->ContourFilter, "ComputeScalars", /*quiet*/ true)
    .Set(1);

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

  // Create the representation for it. Show the unresampled contour filter to
  // start.
  this->ContourRepresentation =
    controller->Show(this->ContourFilter, 0, vtkView);
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Representation")
    .Set("Surface");
  vtkSMPropertyHelper(this->ContourRepresentation, "Position")
    .Set(data->displayPosition(), 3);

  vtkSMPropertyHelper colorArrayHelper(this->ContourRepresentation,
                                       "ColorArrayName");
  this->Internals->NonLabelMapArrayName =
    std::string(colorArrayHelper.GetInputArrayNameToProcess());

  vtkSMPropertyHelper colorHelper(this->ContourRepresentation, "DiffuseColor");
  double white[3] = { 1.0, 1.0, 1.0 };
  colorHelper.Set(white, 3);
  // use proper color map.
  this->updateColorMap();

  this->ContourRepresentation->UpdateVTKObjects();

  return true;
}

void ModuleContour::updateColorMap()
{
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "LookupTable")
    .Set(this->colorMap());
  vtkSMPropertyHelper colorArrayHelper(this->ContourRepresentation,
                                       "ColorArrayName");

  if (this->colorByLabelMap()) {
    this->Internals->NonLabelMapArrayName =
      std::string(colorArrayHelper.GetInputArrayNameToProcess());
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_POINTS, "LabelMap");

    vtkSMPropertyHelper(this->ContourRepresentation, "Input")
      .Set(this->ResampleFilter);
  } else {
    if (this->Internals->UseSolidColor) {
      colorArrayHelper.SetInputArrayToProcess(
        vtkDataObject::FIELD_ASSOCIATION_POINTS, "");
    } else {
      colorArrayHelper.SetInputArrayToProcess(
        vtkDataObject::FIELD_ASSOCIATION_POINTS,
        this->Internals->NonLabelMapArrayName.c_str());
    }
    vtkSMPropertyHelper(this->ContourRepresentation, "Input")
      .Set(this->ContourFilter);
  }

  vtkSMPropertyHelper(this->ContourRepresentation, "Visibility")
    .Set(this->visibility() ? 1 : 0);
  this->ContourRepresentation->UpdateVTKObjects();
}

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

bool ModuleContour::setVisibility(bool val)
{
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Visibility")
    .Set(val ? 1 : 0);
  this->ContourRepresentation->UpdateVTKObjects();

  return true;
}

bool ModuleContour::visibility() const
{
  Q_ASSERT(this->ContourRepresentation);
  return vtkSMPropertyHelper(this->ContourRepresentation, "Visibility")
           .GetAsInt() != 0;
}

void ModuleContour::setIsoValues(const QList<double>& values)
{
  std::vector<double> vectorValues(values.size());
  std::copy(values.begin(), values.end(), vectorValues.begin());
  vectorValues.push_back(0); // to avoid having to check for 0 size on Windows.

  vtkSMPropertyHelper(this->ContourFilter, "ContourValues")
    .Set(&vectorValues[0], values.size());
  this->ContourFilter->UpdateVTKObjects();
}

void ModuleContour::addToPanel(QWidget* panel)
{
  Q_ASSERT(this->ContourFilter);
  Q_ASSERT(this->ContourRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QFormLayout* layout = new QFormLayout;

  QCheckBox* useSolidColor = new QCheckBox;
  useSolidColor->setChecked(this->Internals->UseSolidColor);
  QObject::connect(useSolidColor, &QCheckBox::stateChanged, this,
                   &ModuleContour::setUseSolidColor);
  layout->addRow("Use Solid Color", useSolidColor);

  DoubleSliderWidget* valueSlider = new DoubleSliderWidget(true);
  valueSlider->setLineEditWidth(50);
  layout->addRow("Value", valueSlider);

  QComboBox* representations = new QComboBox;
  representations->addItem("Surface");
  representations->addItem("Wireframe");
  representations->addItem("Points");
  layout->addRow("Representation", representations);
  // TODO connect to update function

  DoubleSliderWidget* opacitySlider = new DoubleSliderWidget(true);
  opacitySlider->setLineEditWidth(50);
  layout->addRow("Opacity", opacitySlider);

  DoubleSliderWidget* specularSlider = new DoubleSliderWidget(true);
  specularSlider->setLineEditWidth(50);
  layout->addRow("Specular", specularSlider);

  pqSignalAdaptorComboBox* adaptor =
    new pqSignalAdaptorComboBox(representations);
  // layout->addStretch();

  QHBoxLayout* rowLayout = new QHBoxLayout;
  rowLayout->addStretch();
  pqColorChooserButton* colorSelector = new pqColorChooserButton(panel);
  colorSelector->setShowAlphaChannel(false);
  rowLayout->addWidget(colorSelector);
  layout->addRow("Color", rowLayout);

  panel->setLayout(layout);

  this->Internals->Links.addPropertyLink(
    valueSlider, "value", SIGNAL(valueEdited(double)), this->ContourFilter,
    this->ContourFilter->GetProperty("ContourValues"), 0);
  new pqWidgetRangeDomain(valueSlider, "minimum", "maximum",
                          this->ContourFilter->GetProperty("ContourValues"), 0);

  this->Internals->Links.addPropertyLink(
    adaptor, "currentText", SIGNAL(currentTextChanged(QString)),
    this->ContourRepresentation,
    this->ContourRepresentation->GetProperty("Representation"));

  this->Internals->Links.addPropertyLink(
    opacitySlider, "value", SIGNAL(valueEdited(double)),
    this->ContourRepresentation,
    this->ContourRepresentation->GetProperty("Opacity"), 0);
  this->Internals->Links.addPropertyLink(
    specularSlider, "value", SIGNAL(valueEdited(double)),
    this->ContourRepresentation,
    this->ContourRepresentation->GetProperty("Specular"), 0);

  this->Internals->Links.addPropertyLink(
    colorSelector, "chosenColorRgbF", SIGNAL(chosenColorChanged(const QColor&)),
    this->ContourRepresentation,
    this->ContourRepresentation->GetProperty("DiffuseColor"));

  this->connect(valueSlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleContour::dataUpdated);
  this->connect(representations, &QComboBox::currentTextChanged, this,
                &ModuleContour::dataUpdated);
  this->connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleContour::dataUpdated);
  this->connect(specularSlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleContour::dataUpdated);
  this->connect(colorSelector, &pqColorChooserButton::chosenColorChanged, this,
                &ModuleContour::dataUpdated);
}

void ModuleContour::dataUpdated()
{
  this->Internals->Links.accept();
  emit this->renderNeeded();
}

bool ModuleContour::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("solid_color").set_value(this->Internals->UseSolidColor);
  // save stuff that the user can change.
  pugi::xml_node node = ns.append_child("ContourFilter");
  QStringList contourProperties;
  contourProperties << "ContourValues";
  if (tomviz::serialize(this->ContourFilter, node, contourProperties) ==
      false) {
    qWarning("Failed to serialize ContourFilter.");
    ns.remove_child(node);
    return false;
  }

  QStringList contourRepresentationProperties;
  contourRepresentationProperties << "Representation"
                                  << "Opacity"
                                  << "Specular"
                                  << "Visibility"
                                  << "DiffuseColor";

  node = ns.append_child("ContourRepresentation");
  if (tomviz::serialize(this->ContourRepresentation, node,
                        contourRepresentationProperties) == false) {
    qWarning("Failed to serialize ContourRepresentation.");
    ns.remove_child(node);
    return false;
  }

  return this->Superclass::serialize(ns);
}

bool ModuleContour::deserialize(const pugi::xml_node& ns)
{
  if (ns.attribute("solid_color")) {
    this->Internals->UseSolidColor = ns.attribute("solid_color").as_bool();
  }
  return tomviz::deserialize(this->ContourFilter, ns.child("ContourFilter")) &&
         tomviz::deserialize(this->ContourRepresentation,
                             ns.child("ContourRepresentation")) &&
         this->Superclass::deserialize(ns);
}

void ModuleContour::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(this->ContourRepresentation, "Position").Set(pos, 3);
  this->ContourRepresentation->MarkDirty(this->ContourRepresentation);
  this->ContourRepresentation->UpdateVTKObjects();
}

bool ModuleContour::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == this->ContourFilter.Get()) ||
         (proxy == this->ContourRepresentation.Get()) ||
         (proxy == this->ResampleFilter.Get());
}

std::string ModuleContour::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == this->ContourFilter.Get()) {
    return "Contour";
  } else if (proxy == this->ContourRepresentation.Get()) {
    return "Representation";
  } else if (proxy == this->ResampleFilter.Get()) {
    return "Resample";
  } else {
    qWarning("Gave bad proxy to module in save animation state");
    return "";
  }
}

vtkSMProxy* ModuleContour::getProxyForString(const std::string& str)
{
  if (str == "Resample") {
    return this->ResampleFilter.Get();
  } else if (str == "Representation") {
    return this->ContourRepresentation.Get();
  } else if (str == "Contour") {
    return this->ContourFilter.Get();
  } else {
    return nullptr;
  }
}

void ModuleContour::setUseSolidColor(int useSolidColor)
{
  this->Internals->UseSolidColor = (useSolidColor != 0);
  this->updateColorMap();
  emit this->renderNeeded();
}

} // end of namespace tomviz
