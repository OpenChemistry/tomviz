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
#include "ModuleContourWidget.h"

#include "ActiveObjects.h"
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

  {
    // Create the representation for the resampled data
    this->ResampleRepresentation =
      controller->Show(this->ResampleFilter, 0, vtkView);
    Q_ASSERT(this->ResampleRepresentation);

    // Set the active representation to resampled data
    this->ActiveRepresentation = this->ResampleRepresentation;

    vtkSMPropertyHelper(this->ResampleRepresentation, "Representation")
      .Set("Surface");
    vtkSMPropertyHelper(this->ResampleRepresentation, "Position")
      .Set(data->displayPosition(), 3);
    this->ResampleRepresentation->UpdateProperty("Visibility");

    vtkSMPropertyHelper colorArrayHelper(this->ResampleRepresentation,
                                         "ColorArrayName");
    this->Internals->ColorArrayName =
      std::string(colorArrayHelper.GetInputArrayNameToProcess());

    vtkSMPropertyHelper colorHelper(this->ResampleRepresentation,
                                    "DiffuseColor");
    double white[3] = { 1.0, 1.0, 1.0 };
    colorHelper.Set(white, 3);
    // use proper color map.
    this->updateColorMap();

    this->ResampleRepresentation->UpdateVTKObjects();
  }

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
  Q_ASSERT(this->ActiveRepresentation);
  vtkSMPropertyHelper(this->ActiveRepresentation, "LookupTable")
    .Set(this->colorMap());

  updateScalarColoring();

  vtkSMPropertyHelper(this->ActiveRepresentation, "Visibility")
    .Set(this->visibility() ? 1 : 0);
  this->ActiveRepresentation->UpdateVTKObjects();
}

bool ModuleContour::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ResampleFilter);
  controller->UnRegisterProxy(this->ResampleRepresentation);
  if (this->PointDataToCellDataFilter) {
    controller->UnRegisterProxy(this->PointDataToCellDataFilter);
  }
  if (this->PointDataToCellDataRepresentation) {
    controller->UnRegisterProxy(this->PointDataToCellDataRepresentation);
  }
  controller->UnRegisterProxy(this->ContourFilter);
  this->ResampleFilter = nullptr;
  this->ContourFilter = nullptr;
  this->ResampleRepresentation = nullptr;
  this->PointDataToCellDataFilter = nullptr;
  this->PointDataToCellDataRepresentation = nullptr;
  this->ActiveRepresentation = nullptr;
  return true;
}

bool ModuleContour::setVisibility(bool val)
{
  Q_ASSERT(this->ActiveRepresentation);
  vtkSMPropertyHelper(this->ActiveRepresentation, "Visibility")
    .Set(val ? 1 : 0);
  this->ActiveRepresentation->UpdateVTKObjects();

  return true;
}

bool ModuleContour::visibility() const
{
  if (this->ActiveRepresentation) {
    return vtkSMPropertyHelper(this->ActiveRepresentation, "Visibility")
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
  Q_ASSERT(this->ResampleRepresentation);

  if (panel->layout()) {
    delete panel->layout();
    m_controllers = nullptr;
  }

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  m_controllers = new ModuleContourWidget;
  layout->addWidget(m_controllers);

  m_controllers->setUseSolidColor(this->Internals->UseSolidColor);

  connect(m_controllers, SIGNAL(useSolidColor(const bool)), this,
          SLOT(setUseSolidColor(const bool)));
  m_controllers->addPropertyLinks(
    this->Internals->Links, this->ResampleRepresentation, this->ContourFilter);
  this->connect(m_controllers, SIGNAL(propertyChanged()), this,
                SLOT(onPropertyChanged()));
  this->connect(this, SIGNAL(dataSourceChanged()), this, SLOT(updateGUI()));

  updateGUI();
  onPropertyChanged();
}

void ModuleContour::createCategoricalColoringPipeline()
{
  if (this->PointDataToCellDataFilter == nullptr) {

    // Set up a point data to cell data filter and set the input data as
    // categorical
    vtkSMSourceProxy* producer =
      this->Internals->ColorByDataSource->producer();

    vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
    vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

    vtkSmartPointer<vtkSMProxy> pdToCdProxy;
    pdToCdProxy.TakeReference(pxm->NewProxy("filters","PointDataToCellData"));

    this->PointDataToCellDataFilter =
      vtkSMSourceProxy::SafeDownCast(pdToCdProxy);
    Q_ASSERT(this->PointDataToCellDataFilter);
    controller->PreInitializeProxy(this->PointDataToCellDataFilter);
    vtkSMPropertyHelper(this->PointDataToCellDataFilter, "Input")
      .Set(this->ResampleFilter);
    vtkSMPropertyHelper(this->PointDataToCellDataFilter,
                        "CategoricalData").Set(1);
    controller->PostInitializeProxy(this->PointDataToCellDataFilter);
    controller->RegisterPipelineProxy(this->PointDataToCellDataFilter);
  }

  if (this->PointDataToCellDataRepresentation == nullptr) {

    // Create the representation for the point data to cell data
    vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

    this->PointDataToCellDataRepresentation =
      controller->Show(this->PointDataToCellDataFilter, 0,
                       ActiveObjects::instance().activeView());
    Q_ASSERT(this->PointDataToCellDataRepresentation);
    vtkSMPropertyHelper(this->PointDataToCellDataRepresentation,
                        "Representation")
      .Set("Surface");
    vtkSMPropertyHelper(this->PointDataToCellDataRepresentation, "Position")
      .Set(this->Internals->ColorByDataSource->displayPosition(), 3);

    vtkSMPropertyHelper(this->PointDataToCellDataRepresentation,
                        "Visibility").Set(0);

    vtkSMPropertyHelper colorArrayHelper(
      this->PointDataToCellDataRepresentation, "ColorArrayName");
    this->Internals->ColorArrayName =
      std::string(colorArrayHelper.GetInputArrayNameToProcess());

    vtkSMPropertyHelper colorHelper(this->PointDataToCellDataRepresentation,
                                    "DiffuseColor");
    double white[3] = { 1.0, 1.0, 1.0 };
    colorHelper.Set(white, 3);

    this->PointDataToCellDataRepresentation->UpdateVTKObjects();

    m_controllers->addCategoricalPropertyLinks(
      this->Internals->Links, this->PointDataToCellDataRepresentation);
  }
}

void ModuleContour::onPropertyChanged()
{
  this->Internals->Links.accept();

  int colorByIndex = m_controllers->getColorByComboBox()->currentIndex();
  if (colorByIndex > 0) {
    createCategoricalColoringPipeline();
    auto childDataSources = getChildDataSources();
    this->Internals->ColorByDataSource = childDataSources[colorByIndex - 1];
    vtkSMPropertyHelper(this->ResampleFilter, "CategoricalData").Set(1);
    vtkSMPropertyHelper(this->ResampleRepresentation, "Visibility").Set(0);
    this->ResampleRepresentation->UpdateProperty("Visibility");
    this->ActiveRepresentation = this->PointDataToCellDataRepresentation;
  } else {
    this->Internals->ColorByDataSource = dataSource();
    vtkSMPropertyHelper(this->ResampleFilter, "CategoricalData").Set(0);
    if (this->PointDataToCellDataRepresentation) {
      vtkSMPropertyHelper(this->PointDataToCellDataRepresentation,
                          "Visibility").Set(0);
      this->PointDataToCellDataRepresentation->UpdateProperty("Visibility");
    }
    this->ActiveRepresentation = this->ResampleRepresentation;
  }
  setVisibility(true);

  vtkSMPropertyHelper resampleHelper(this->ResampleFilter, "Input");
  resampleHelper.Set(this->Internals->ColorByDataSource->producer());

  updateScalarColoring();

  this->ResampleFilter->UpdateVTKObjects();
  if (this->PointDataToCellDataFilter) {
    this->PointDataToCellDataFilter->UpdateVTKObjects();
  }
  this->ActiveRepresentation->MarkDirty(this->ActiveRepresentation);
  this->ActiveRepresentation->UpdateVTKObjects();

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

  {
    QStringList resampleRepresentationProperties;
    resampleRepresentationProperties << "Representation"
                                    << "Opacity"
                                    << "Specular"
                                    << "Visibility"
                                    << "DiffuseColor"
                                    << "AmbientColor";

    node = ns.append_child("ResampleRepresentation");
    if (tomviz::serialize(this->ResampleRepresentation, node,
                          resampleRepresentationProperties) == false) {
      qWarning("Failed to serialize ResampleRepresentation.");
      ns.remove_child(node);
      return false;
    }
  }

  if (this->PointDataToCellDataRepresentation) {
    QStringList pointDataToCellDataRepresentationProperties;
    pointDataToCellDataRepresentationProperties << "Representation"
                                    << "Opacity"
                                    << "Specular"
                                    << "Visibility"
                                    << "DiffuseColor"
                                    << "AmbientColor";

    node = ns.append_child("PointDataToCellDataRepresentation");
    if (tomviz::serialize(this->PointDataToCellDataRepresentation, node,
                          pointDataToCellDataRepresentationProperties)==false) {
      qWarning("Failed to serialize PointDataToCellDataRepresentation.");
      ns.remove_child(node);
      return false;
    }
  }

  return this->Superclass::serialize(ns);
}

bool ModuleContour::deserialize(const pugi::xml_node& ns)
{
  if (ns.attribute("solid_color")) {
    this->Internals->UseSolidColor = ns.attribute("solid_color").as_bool();
  }
  if (ns.child("PointDataToCellDataRepresentation")) {
      createCategoricalColoringPipeline();
      if (!tomviz::deserialize(this->PointDataToCellDataRepresentation,
                               ns.child("PointDataToCellDataRepresentation"))) {
        return false;
      }
    }

  return tomviz::deserialize(this->ContourFilter, ns.child("ContourFilter")) &&
         tomviz::deserialize(this->ResampleRepresentation,
                             ns.child("ResampleRepresentation")) &&
         this->Superclass::deserialize(ns);
}

void ModuleContour::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(this->ActiveRepresentation, "Position").Set(pos, 3);
  this->ActiveRepresentation->MarkDirty(this->ActiveRepresentation);
  this->ActiveRepresentation->UpdateVTKObjects();
}

bool ModuleContour::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == this->ContourFilter.Get()) ||
         (proxy == this->ResampleRepresentation.Get()) ||
         (this->PointDataToCellDataRepresentation &&
          proxy == this->PointDataToCellDataRepresentation.Get()) ||
         (proxy == this->ResampleFilter.Get());
}

std::string ModuleContour::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == this->ContourFilter.Get()) {
    return "Contour";
  } else if (proxy == this->ResampleFilter.Get()) {
    return "Resample";
  } else if (this->PointDataToCellDataFilter &&
             proxy == this->PointDataToCellDataFilter.Get()) {
    return "PointDataToCellData";
  } else if (proxy == this->ResampleRepresentation.Get()) {
    return "ResampleRepresentation";
  } else if (this->PointDataToCellDataRepresentation &&
             proxy == this->PointDataToCellDataRepresentation.Get()) {
    return "PointDataToCellDataRepresentation";
  } else {
    qWarning("Gave bad proxy to module in save animation state");
    return "";
  }
}

vtkSMProxy* ModuleContour::getProxyForString(const std::string& str)
{
  if (str == "Resample") {
    return this->ResampleFilter.Get();
  } else if (str == "Contour") {
    return this->ContourFilter.Get();
  } else if (this->PointDataToCellDataFilter &&
             str == "PointDataToCellData") {
    return this->PointDataToCellDataFilter.Get();
  } else if (str == "ResampleRepresentation") {
    return this->ResampleRepresentation.Get();
  } else if (this->PointDataToCellDataRepresentation &&
             str == "PointDataToCellDataRepresentation") {
    return this->PointDataToCellDataRepresentation.Get();
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

  vtkSMPropertyHelper colorArrayHelper(this->ActiveRepresentation,
                                       "ColorArrayName");
  if (this->Internals->UseSolidColor) {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_POINTS, "");
  } else if (m_controllers->getColorByComboBox()->currentIndex() > 0) {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_CELLS, arrayName.c_str());
  } else {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_POINTS, arrayName.c_str());
  }
}

void ModuleContour::setUseSolidColor(const bool useSolidColor)
{
  this->Internals->UseSolidColor = useSolidColor;
  this->updateColorMap();
  emit this->renderNeeded();
}

void ModuleContour::updateGUI()
{
  QList<DataSource*> childSources = getChildDataSources();
  QComboBox* combo = m_controllers->getColorByComboBox();
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
