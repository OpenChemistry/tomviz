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
#include "Operator.h"
#include "Utilities.h"

#include "pqColorChooserButton.h"
#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqWidgetRangeDomain.h"

#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkSMPVRepresentationProxy.h"
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
#include <QPointer>

namespace tomviz {

class ModuleContour::Private
{
public:
  std::string ColorArrayName;
  bool UseSolidColor;
  QPointer<QComboBox> ColorByComboBox;
  pqPropertyLinks Links;
  QPointer<DataSource> ColorByDataSource;
};

ModuleContour::ModuleContour(QObject* parentObject) : Superclass(parentObject)
{
  this->Internals = new Private;
  this->Internals->Links.setAutoUpdateVTKObjects(true);
  this->Internals->UseSolidColor = false;
  this->Internals->ColorByDataSource = nullptr;
}

ModuleContour::~ModuleContour()
{
  this->finalize();

  delete this->Internals;
  this->Internals = nullptr;
}

QIcon ModuleContour::icon() const
{
  return QIcon(":/icons/pqIsosurface.png");
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

  // Set up a data resampler to add LabelMap values on the contour, and mark
  // the LabelMap as categorical
  vtkSmartPointer<vtkSMProxy> probeProxy;
  probeProxy.TakeReference(pxm->NewProxy("filters", "ResampleWithDataset"));

  this->ResampleFilter = vtkSMSourceProxy::SafeDownCast(probeProxy);
  Q_ASSERT(this->ResampleFilter);
  controller->PreInitializeProxy(this->ResampleFilter);
  vtkSMPropertyHelper(this->ResampleFilter, "Input").Set(data->producer());
  vtkSMPropertyHelper(this->ResampleFilter, "Source").Set(this->ContourFilter);
  vtkSMPropertyHelper(this->ResampleFilter, "CategoricalData").Set(1);
  vtkSMPropertyHelper(this->ResampleFilter, "PassPointArrays").Set(1);
  controller->PostInitializeProxy(this->ResampleFilter);
  controller->RegisterPipelineProxy(this->ResampleFilter);

  // Set up a point data to cell data filter and set the input data as
  // categorical
  vtkSmartPointer<vtkSMProxy> pdToCdProxy;
  pdToCdProxy.TakeReference(pxm->NewProxy("filters", "PointDataToCellData"));

  this->PointDataToCellDataFilter = vtkSMSourceProxy::SafeDownCast(pdToCdProxy);
  Q_ASSERT(this->PointDataToCellDataFilter);
  controller->PreInitializeProxy(this->PointDataToCellDataFilter);
  vtkSMPropertyHelper(this->PointDataToCellDataFilter, "Input")
    .Set(this->ResampleFilter);
  vtkSMPropertyHelper(this->PointDataToCellDataFilter, "PassPointData").Set(1);
  vtkSMPropertyHelper(this->PointDataToCellDataFilter,
                      "CategoricalData").Set(1);
  controller->PostInitializeProxy(this->PointDataToCellDataFilter);
  controller->RegisterPipelineProxy(this->PointDataToCellDataFilter);

  // Create the representation for it. Show the unresampled contour filter to
  // start.
  this->ContourRepresentation =
    controller->Show(this->PointDataToCellDataFilter, 0, vtkView);
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Representation")
    .Set("Surface");
  vtkSMPropertyHelper(this->ContourRepresentation, "Position")
    .Set(data->displayPosition(), 3);

  vtkSMPropertyHelper colorArrayHelper(this->ContourRepresentation,
                                       "ColorArrayName");
  this->Internals->ColorArrayName =
    std::string(colorArrayHelper.GetInputArrayNameToProcess());

  vtkSMPropertyHelper colorHelper(this->ContourRepresentation, "DiffuseColor");
  double white[3] = { 1.0, 1.0, 1.0 };
  colorHelper.Set(white, 3);
  // use proper color map.
  this->updateColorMap();

  this->ContourRepresentation->UpdateVTKObjects();

  // Color by the data source by default
  this->Internals->ColorByDataSource = dataSource();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(contourProxy)) {
    p->rename(label());
  }

  return true;
}

void ModuleContour::updateColorMap()
{
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "LookupTable")
    .Set(this->colorMap());

  updateScalarColoring();

  vtkSMPropertyHelper(this->ContourRepresentation, "Visibility")
    .Set(this->visibility() ? 1 : 0);
  this->ContourRepresentation->UpdateVTKObjects();
}

bool ModuleContour::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->PointDataToCellDataFilter);
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
  if (this->ContourRepresentation) {
    return vtkSMPropertyHelper(this->ContourRepresentation, "Visibility")
             .GetAsInt() != 0;
  } else {
    return false;
  }
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
  Q_ASSERT(this->ResampleFilter);
  Q_ASSERT(this->ContourRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

//  QVBoxLayout* layout = new QVBoxLayout;
//  panel->setLayout(layout);
//
//  m_controllers = new ModuleContourWidget;
//  layout->addWidget(m_controllers);
//
//  m_controllers->setLighting(static_cast<bool>(m_propertyroperty->GetShade()));
//  m_controllers->setAmbient(m_volumeProperty->GetAmbient());
//  m_controllers->setDiffuse(m_volumeProperty->GetDiffuse());
//  m_controllers->setSpecular(m_volumeProperty->GetSpecular());
//  m_controllers->setSpecularPower(m_volumeProperty->GetSpecularPower());

/////////////////////////////////////
  QFormLayout* layout = new QFormLayout;

  // Solid color
  QHBoxLayout* colorLayout = new QHBoxLayout;
  colorLayout->addStretch();

  QCheckBox* useSolidColor = new QCheckBox;
  useSolidColor->setChecked(this->Internals->UseSolidColor);
  QObject::connect(useSolidColor, &QCheckBox::stateChanged, this,
                   &ModuleContour::setUseSolidColor);
  colorLayout->addWidget(useSolidColor);

  QLabel* colorLabel = new QLabel("Select Color");
  colorLayout->addWidget(colorLabel);

  pqColorChooserButton* colorSelector = new pqColorChooserButton(panel);
  colorLayout->addWidget(colorSelector);
  layout->addRow("", colorLayout);

  if (this->Internals->ColorByComboBox.isNull()) {
    this->Internals->ColorByComboBox = new QComboBox();
  }
  layout->addRow("Color By", this->Internals->ColorByComboBox);

  colorSelector->setShowAlphaChannel(false);
  DoubleSliderWidget* valueSlider = new DoubleSliderWidget(true);
  valueSlider->setLineEditWidth(50);
  layout->addRow("Value", valueSlider);

  QComboBox* representations = new QComboBox;
  representations->addItem("Surface");
  representations->addItem("Wireframe");
  representations->addItem("Points");
  layout->addRow("", representations);
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

  // Surface uses DiffuseColor and Wireframe uses AmbientColor so we have to set
  // both
  this->Internals->Links.addPropertyLink(
    colorSelector, "chosenColorRgbF", SIGNAL(chosenColorChanged(const QColor&)),
    this->ContourRepresentation,
    this->ContourRepresentation->GetProperty("DiffuseColor"));
  this->Internals->Links.addPropertyLink(
    colorSelector, "chosenColorRgbF", SIGNAL(chosenColorChanged(const QColor&)),
    this->ContourRepresentation,
    this->ContourRepresentation->GetProperty("AmbientColor"));

  this->connect(valueSlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleContour::propertyChanged);
  this->connect(representations, &QComboBox::currentTextChanged, this,
                &ModuleContour::propertyChanged);
  this->connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleContour::propertyChanged);
  this->connect(specularSlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleContour::propertyChanged);
  this->connect(colorSelector, &pqColorChooserButton::chosenColorChanged, this,
                &ModuleContour::propertyChanged);
  this->connect(this->Internals->ColorByComboBox,
                SIGNAL(currentIndexChanged(int)), this,
                SLOT(propertyChanged()));

  this->connect(this, SIGNAL(dataSourceChanged()), this, SLOT(updateGUI()));

  updateGUI();
  propertyChanged();
/////////////////////////////////////
}

void ModuleContour::propertyChanged()
{
  this->Internals->Links.accept();

  int colorByIndex = this->Internals->ColorByComboBox->currentIndex();
  if (colorByIndex > 0) {
    auto childDataSources = getChildDataSources();
    this->Internals->ColorByDataSource = childDataSources[colorByIndex - 1];
  } else {
    this->Internals->ColorByDataSource = dataSource();
  }

  vtkSMPropertyHelper resampleHelper(this->ResampleFilter, "Input");
  resampleHelper.Set(this->Internals->ColorByDataSource->producer());

  updateScalarColoring();

  this->ResampleFilter->UpdateVTKObjects();
  this->PointDataToCellDataFilter->UpdateVTKObjects();
  this->ContourRepresentation->MarkDirty(this->ContourRepresentation);
  this->ContourRepresentation->UpdateVTKObjects();

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
                                  << "DiffuseColor"
                                  << "AmbientColor";

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
  } else if (proxy == this->PointDataToCellDataFilter.Get()) {
    return "PointDataToCellData";
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
  } else if (str == "PointDataToCellData") {
    return this->PointDataToCellDataFilter.Get();
  } else {
    return nullptr;
  }
}

QList<DataSource*> ModuleContour::getChildDataSources()
{
  QList<DataSource*> childSources;
  auto source = dataSource();
  if (!source) {
    return childSources;
  }

  // Iterate over Operators and obtain child data sources
  auto ops = source->operators();
  for (int i = 0; i < ops.size(); ++i) {
    auto op = ops[i];
    if (!op || !op->hasChildDataSource()) {
      continue;
    }

    auto child = op->childDataSource();
    if (!child) {
      continue;
    }

    childSources.append(child);
  }

  return childSources;
}

void ModuleContour::updateScalarColoring()
{
  if (!this->Internals->ColorByDataSource) {
    return;
  }

  std::string arrayName(this->Internals->ColorArrayName);

  // Get the active point scalars from the resample filter
  vtkPVDataInformation* dataInfo = nullptr;
  vtkPVDataSetAttributesInformation* attributeInfo = nullptr;
  vtkPVArrayInformation* arrayInfo = nullptr;
  if (this->Internals->ColorByDataSource) {
    dataInfo =
      this->Internals->ColorByDataSource->producer()->GetDataInformation(0);
  }
  if (dataInfo) {
    attributeInfo = dataInfo->GetAttributeInformation(
      vtkDataObject::FIELD_ASSOCIATION_POINTS);
  }
  if (attributeInfo) {
    arrayInfo =
      attributeInfo->GetAttributeInformation(vtkDataSetAttributes::SCALARS);
  }
  if (arrayInfo) {
    arrayName = arrayInfo->GetName();
  }

  vtkSMPropertyHelper colorArrayHelper(this->ContourRepresentation,
                                       "ColorArrayName");
  if (this->Internals->UseSolidColor) {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_POINTS, "");
  } else {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_CELLS, arrayName.c_str());
  }
}

void ModuleContour::setUseSolidColor(int useSolidColor)
{
  this->Internals->UseSolidColor = (useSolidColor != 0);
  this->updateColorMap();
  emit this->renderNeeded();
}

void ModuleContour::updateGUI()
{
  QList<DataSource*> childSources = getChildDataSources();
  QComboBox* combo = this->Internals->ColorByComboBox;
  if (combo) {
    combo->blockSignals(true);
    combo->clear();
    combo->addItem("This Data");
    for (int i = 0; i < childSources.size(); ++i) {
      combo->addItem(childSources[i]->filename());
    }

    int selected = childSources.indexOf(this->Internals->ColorByDataSource);

    // If data source not found, selected will be -1, so the current index will
    // be set to 0, which is the right index for this data source.
    combo->setCurrentIndex(selected + 1);
    combo->blockSignals(false);
  }
}

} // end of namespace tomviz
