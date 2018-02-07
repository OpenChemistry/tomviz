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

#include "vtkAlgorithm.h"
#include "vtkDataArray.h"
#include "vtkDataObject.h"
#include "vtkDataSet.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTransferFunctionProxy.h"
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
  bool UseSolidColor = false;
  pqPropertyLinks Links;
};

ModuleContour::ModuleContour(QObject* parentObject) : Module(parentObject)
{
  d = new Private;
  d->Links.setAutoUpdateVTKObjects(true);
}

ModuleContour::~ModuleContour()
{
  finalize();

  delete d;
  d = nullptr;
}

QIcon ModuleContour::icon() const
{
  return QIcon(":/icons/pqIsosurface.png");
}

bool ModuleContour::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  auto producer = data->proxy();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  vtkSmartPointer<vtkSMProxy> contourProxy;
  contourProxy.TakeReference(pxm->NewProxy("filters", "FlyingEdges"));

  m_contourFilter = vtkSMSourceProxy::SafeDownCast(contourProxy);
  Q_ASSERT(m_contourFilter);
  controller->PreInitializeProxy(m_contourFilter);
  vtkSMPropertyHelper(m_contourFilter, "Input").Set(producer);
  vtkSMPropertyHelper(m_contourFilter, "ComputeScalars", /*quiet*/ true)
    .Set(1);

  controller->PostInitializeProxy(m_contourFilter);
  controller->RegisterPipelineProxy(m_contourFilter);

  // Set up a data resampler to add LabelMap values on the contour, and mark
  // the LabelMap as categorical
  vtkSmartPointer<vtkSMProxy> probeProxy;
  probeProxy.TakeReference(pxm->NewProxy("filters", "ResampleWithDataset"));

  m_resampleFilter = vtkSMSourceProxy::SafeDownCast(probeProxy);
  Q_ASSERT(m_resampleFilter);
  controller->PreInitializeProxy(m_resampleFilter);
  vtkSMPropertyHelper(m_resampleFilter, "Input").Set(data->proxy());
  vtkSMPropertyHelper(m_resampleFilter, "Source").Set(m_contourFilter);
  vtkSMPropertyHelper(m_resampleFilter, "PassPointArrays").Set(1);
  controller->PostInitializeProxy(m_resampleFilter);
  controller->RegisterPipelineProxy(m_resampleFilter);

  {
    // Create the representation for the resampled data
    m_resampleRepresentation =
      controller->Show(m_resampleFilter, 0, vtkView);
    Q_ASSERT(m_resampleRepresentation);

    // Set the active representation to resampled data
    m_activeRepresentation = m_resampleRepresentation;

    vtkSMPropertyHelper(m_resampleRepresentation, "Representation")
      .Set("Surface");
    vtkSMPropertyHelper(m_resampleRepresentation, "Position")
      .Set(data->displayPosition(), 3);
    m_resampleRepresentation->UpdateProperty("Visibility");

    vtkSMPropertyHelper colorHelper(m_resampleRepresentation,
                                    "DiffuseColor");
    double white[3] = { 1.0, 1.0, 1.0 };
    colorHelper.Set(white, 3);
    // use proper color map.
    updateColorMap();

    m_resampleRepresentation->UpdateVTKObjects();
  }

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(contourProxy)) {
    p->rename(label());
  }

  connect(data, SIGNAL(activeScalarsChanged()), SLOT(onScalarArrayChanged()));
  onScalarArrayChanged();

  connect(this, SIGNAL(colorMapChanged()), this, SLOT(updateRangeSliders()));

  return true;
}

void ModuleContour::updateColorMap()
{
  Q_ASSERT(m_activeRepresentation);
  vtkSMPropertyHelper(m_activeRepresentation, "LookupTable")
    .Set(colorMap());

  updateScalarColoring();

  vtkSMPropertyHelper(m_activeRepresentation, "Visibility")
    .Set(visibility() ? 1 : 0);
  m_activeRepresentation->UpdateVTKObjects();
}

bool ModuleContour::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_resampleFilter);
  controller->UnRegisterProxy(m_resampleRepresentation);
  if (m_pointDataToCellDataFilter) {
    controller->UnRegisterProxy(m_pointDataToCellDataFilter);
  }
  if (m_pointDataToCellDataRepresentation) {
    controller->UnRegisterProxy(m_pointDataToCellDataRepresentation);
  }
  controller->UnRegisterProxy(m_contourFilter);
  m_resampleFilter = nullptr;
  m_contourFilter = nullptr;
  m_resampleRepresentation = nullptr;
  m_pointDataToCellDataFilter = nullptr;
  m_pointDataToCellDataRepresentation = nullptr;
  m_activeRepresentation = nullptr;
  return true;
}

bool ModuleContour::setVisibility(bool val)
{
  Q_ASSERT(m_activeRepresentation);
  vtkSMPropertyHelper(m_activeRepresentation, "Visibility")
    .Set(val ? 1 : 0);
  m_activeRepresentation->UpdateVTKObjects();

  return true;
}

bool ModuleContour::visibility() const
{
  if (m_activeRepresentation) {
    return vtkSMPropertyHelper(m_activeRepresentation, "Visibility")
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

  vtkSMPropertyHelper(m_contourFilter, "ContourValues")
    .Set(&vectorValues[0], values.size());
  m_contourFilter->UpdateVTKObjects();

  updateScalarColoring();
}

void ModuleContour::addToPanel(QWidget* panel)
{
  Q_ASSERT(m_contourFilter);
  Q_ASSERT(m_resampleFilter);
  Q_ASSERT(m_resampleRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  m_controllers = new ModuleContourWidget;
  layout->addWidget(m_controllers);

  m_controllers->setUseSolidColor(d->UseSolidColor);

  connect(m_controllers, SIGNAL(useSolidColor(const bool)), this,
          SLOT(setUseSolidColor(const bool)));
  m_controllers->addPropertyLinks(
    d->Links, m_resampleRepresentation, m_contourFilter);
  connect(m_controllers, SIGNAL(propertyChanged()), this,
                SLOT(onPropertyChanged()));
  connect(this, SIGNAL(dataSourceChanged()), this, SLOT(updateGUI()));
  connect(m_controllers->getColorByComboBox(), SIGNAL(currentIndexChanged(int)),
          this, SIGNAL(colorMapChanged()));

  updateGUI();
  onPropertyChanged();
}

void ModuleContour::createCategoricalColoringPipeline()
{
  if (m_pointDataToCellDataFilter == nullptr) {

    // Set up a point data to cell data filter and set the input data as
    // categorical
    vtkSMSourceProxy* producer = dataSource()->proxy();

    vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
    vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

    vtkSmartPointer<vtkSMProxy> pdToCdProxy;
    pdToCdProxy.TakeReference(pxm->NewProxy("filters", "PointDataToCellData"));

    m_pointDataToCellDataFilter =
      vtkSMSourceProxy::SafeDownCast(pdToCdProxy);
    Q_ASSERT(m_pointDataToCellDataFilter);
    controller->PreInitializeProxy(m_pointDataToCellDataFilter);
    vtkSMPropertyHelper(m_pointDataToCellDataFilter, "Input")
      .Set(m_resampleFilter);
    vtkSMPropertyHelper(m_pointDataToCellDataFilter, "CategoricalData")
      .Set(1);
    controller->PostInitializeProxy(m_pointDataToCellDataFilter);
    controller->RegisterPipelineProxy(m_pointDataToCellDataFilter);
  }

  if (m_pointDataToCellDataRepresentation == nullptr) {

    // Create the representation for the point data to cell data
    vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

    m_pointDataToCellDataRepresentation =
      controller->Show(m_pointDataToCellDataFilter, 0,
                       ActiveObjects::instance().activeView());
    Q_ASSERT(m_pointDataToCellDataRepresentation);
    vtkSMPropertyHelper(m_pointDataToCellDataRepresentation,
                        "Representation")
      .Set("Surface");
    vtkSMPropertyHelper(m_pointDataToCellDataRepresentation, "Position")
      .Set(dataSource()->displayPosition(), 3);

    vtkSMPropertyHelper(m_pointDataToCellDataRepresentation, "Visibility")
      .Set(0);

    vtkSMPropertyHelper colorHelper(m_pointDataToCellDataRepresentation,
                                    "DiffuseColor");
    double white[3] = { 1.0, 1.0, 1.0 };
    colorHelper.Set(white, 3);

    m_pointDataToCellDataRepresentation->UpdateVTKObjects();

    if (m_controllers) {
      m_controllers->addCategoricalPropertyLinks(
        d->Links, m_pointDataToCellDataRepresentation);
    }
  }
}

void ModuleContour::onPropertyChanged()
{
  d->Links.accept();

  if (!m_controllers) {
    return;
  }

  m_activeRepresentation = m_resampleRepresentation;

  auto comboBox = m_controllers->getColorByComboBox();
  auto arrayName = comboBox->currentText();

  vtkSMPropertyHelper(m_activeRepresentation, "ColorArrayName")
    .SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS,
                            arrayName.toLatin1().data());

  // Update the range sliders
  updateRangeSliders();

  // Rescale the current color map
  double range[2] = {0, 0};
  m_controllers->getColorMapRange(range);
  auto cmap = colorMap();
  auto omap = opacityMap();
  vtkSMTransferFunctionProxy::RescaleTransferFunction(cmap, range, false /*extend*/);
  vtkSMTransferFunctionProxy::RescaleTransferFunction(omap, range, false /*extend*/);
  //emit colorMapChanged();

  setVisibility(true);

  vtkSMPropertyHelper resampleHelper(m_resampleFilter, "Input");
  resampleHelper.Set(dataSource()->proxy());

  updateColorMap();

  m_resampleFilter->UpdateVTKObjects();
  if (m_pointDataToCellDataFilter) {
    m_pointDataToCellDataFilter->UpdateVTKObjects();
  }
  m_activeRepresentation->MarkDirty(m_activeRepresentation);
  m_activeRepresentation->UpdateVTKObjects();

  emit renderNeeded();
}

void ModuleContour::onScalarArrayChanged()
{
  QString arrayName = dataSource()->activeScalars();
  vtkSMPropertyHelper(m_contourFilter, "SelectInputScalars")
    .SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS,
                            arrayName.toLatin1().data());
  m_contourFilter->UpdateVTKObjects();

  onPropertyChanged();

  emit renderNeeded();
}

bool ModuleContour::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("solid_color").set_value(d->UseSolidColor);
  // save stuff that the user can change.
  pugi::xml_node node = ns.append_child("ContourFilter");
  QStringList contourProperties;
  contourProperties << "ContourValues";
  if (tomviz::serialize(m_contourFilter, node, contourProperties) ==
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
                                     << "MapScalars"
                                     << "DiffuseColor"
                                     << "AmbientColor"
                                     << "Ambient"
                                     << "Diffuse"
                                     << "SpecularPower";

    node = ns.append_child("ResampleRepresentation");
    if (tomviz::serialize(m_resampleRepresentation, node,
                          resampleRepresentationProperties) == false) {
      qWarning("Failed to serialize ResampleRepresentation.");
      ns.remove_child(node);
      return false;
    }
  }

  if (m_pointDataToCellDataRepresentation) {
    QStringList pointDataToCellDataRepresentationProperties;
    pointDataToCellDataRepresentationProperties << "Representation"
                                                << "Opacity"
                                                << "Specular"
                                                << "Visibility"
                                                << "MapScalars"
                                                << "DiffuseColor"
                                                << "AmbientColor"
                                                << "Ambient"
                                                << "Diffuse"
                                                << "SpecularPower";

    node = ns.append_child("PointDataToCellDataRepresentation");
    if (tomviz::serialize(m_pointDataToCellDataRepresentation, node,
                          pointDataToCellDataRepresentationProperties) ==
        false) {
      qWarning("Failed to serialize PointDataToCellDataRepresentation.");
      ns.remove_child(node);
      return false;
    }
  }

  return Module::serialize(ns);
}

bool ModuleContour::deserialize(const pugi::xml_node& ns)
{
  if (ns.attribute("solid_color")) {
    d->UseSolidColor = ns.attribute("solid_color").as_bool();
  }
  if (ns.child("PointDataToCellDataRepresentation")) {
    createCategoricalColoringPipeline();
    if (!tomviz::deserialize(m_pointDataToCellDataRepresentation,
                             ns.child("PointDataToCellDataRepresentation"))) {
      return false;
    }
  }

  return tomviz::deserialize(m_contourFilter, ns.child("ContourFilter")) &&
         tomviz::deserialize(m_resampleRepresentation,
                             ns.child("ResampleRepresentation")) &&
         Module::deserialize(ns);
}

void ModuleContour::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_activeRepresentation, "Position").Set(pos, 3);
  m_activeRepresentation->MarkDirty(m_activeRepresentation);
  m_activeRepresentation->UpdateVTKObjects();
}

DataSource* ModuleContour::colorMapDataSource() const
{
  return dataSource();
}

bool ModuleContour::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == m_contourFilter.Get()) ||
         (proxy == m_resampleRepresentation.Get()) ||
         (m_pointDataToCellDataRepresentation &&
          proxy == m_pointDataToCellDataRepresentation.Get()) ||
         (proxy == m_resampleFilter.Get());
}

vtkSmartPointer<vtkDataObject> ModuleContour::getDataToExport()
{
  return vtkAlgorithm::SafeDownCast(m_contourFilter->GetClientSideObject())
    ->GetOutputDataObject(0);
}

std::string ModuleContour::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_contourFilter.Get()) {
    return "Contour";
  } else if (proxy == m_resampleFilter.Get()) {
    return "Resample";
  } else if (m_pointDataToCellDataFilter &&
             proxy == m_pointDataToCellDataFilter.Get()) {
    return "PointDataToCellData";
  } else if (proxy == m_resampleRepresentation.Get()) {
    return "ResampleRepresentation";
  } else if (m_pointDataToCellDataRepresentation &&
             proxy == m_pointDataToCellDataRepresentation.Get()) {
    return "PointDataToCellDataRepresentation";
  } else {
    qWarning("Gave bad proxy to module in save animation state");
    return "";
  }
}

vtkSMProxy* ModuleContour::getProxyForString(const std::string& str)
{
  if (str == "Resample") {
    return m_resampleFilter.Get();
  } else if (str == "Contour") {
    return m_contourFilter.Get();
  } else if (m_pointDataToCellDataFilter && str == "PointDataToCellData") {
    return m_pointDataToCellDataFilter.Get();
  } else if (str == "ResampleRepresentation") {
    return m_resampleRepresentation.Get();
  } else if (m_pointDataToCellDataRepresentation &&
             str == "PointDataToCellDataRepresentation") {
    return m_pointDataToCellDataRepresentation.Get();
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
  ActiveObjects::instance().colorMapChanged(colorMapDataSource());
}

void ModuleContour::setUseSolidColor(const bool useSolidColor)
{
  d->UseSolidColor = useSolidColor;
  updateColorMap();
  emit renderNeeded();
}

void ModuleContour::updateRangeSliders()
{
  auto comboBox = m_controllers->getColorByComboBox();
  auto arrayName = comboBox->currentText();

  double dataRange[2] = {0, 0};
  auto dataset = vtkDataSet::SafeDownCast(colorMapDataSource()->dataObject());
  auto colorArray = dataset->GetPointData()->GetArray(arrayName.toLatin1().data());
  if (colorArray) {
    colorArray->GetRange(dataRange, -1);
  }

  m_controllers->setColorMapRangeDomain(dataRange);

  // Get the range of the lookup table
  double range[2] = {0, 0};
  vtkSMTransferFunctionProxy::GetRange(colorMap(), range);
  m_controllers->setColorMapRange(range);
}

void ModuleContour::updateGUI()
{
  if (!m_controllers) {
    return;
  }
  QList<DataSource*> childSources = getChildDataSources();
  QComboBox* combo = m_controllers->getColorByComboBox();
  if (combo) {
    combo->blockSignals(true);
    combo->clear();

    auto dataSet = vtkDataSet::SafeDownCast(dataSource()->dataObject());
    auto pointData = dataSet->GetPointData();
    for (int i = 0; i < pointData->GetNumberOfArrays(); ++i) {
      combo->addItem(pointData->GetArray(i)->GetName());
    }

    // Get the active ColorArrayName
    vtkSMPropertyHelper colorArrayHelper(
      m_activeRepresentation, "ColorArrayName");
    auto colorArrayName =
      QString(colorArrayHelper.GetInputArrayNameToProcess());
    combo->setCurrentText(colorArrayName);
    combo->blockSignals(false);

    // Get the data range sliders
    updateRangeSliders();

    // Get the color map data range
    auto colorMap = colorMapDataSource()->colorMap();
    double range[2];
    vtkSMTransferFunctionProxy::GetRange(colorMap, range);
    m_controllers->setColorMapRange(range);
  }
}

} // end of namespace tomviz
