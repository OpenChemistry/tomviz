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
#include <functional>
#include <string>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>

namespace tomviz {

class ModuleContour::Private
{
public:
  std::string ColorArrayName;
  bool UseSolidColor = false;
  pqPropertyLinks Links;
  QPointer<DataSource> ColorByDataSource = nullptr;
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
  vtkSMPropertyHelper(m_resampleFilter, "CategoricalData").Set(1);
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

    vtkSMPropertyHelper colorArrayHelper(m_resampleRepresentation,
                                         "ColorArrayName");
    d->ColorArrayName =
      std::string(colorArrayHelper.GetInputArrayNameToProcess());

    vtkSMPropertyHelper colorHelper(m_resampleRepresentation,
                                    "DiffuseColor");
    double white[3] = { 1.0, 1.0, 1.0 };
    colorHelper.Set(white, 3);
    // use proper color map.
    updateColorMap();

    m_resampleRepresentation->UpdateVTKObjects();
  }

  // Color by the data source by default
  d->ColorByDataSource = dataSource();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(contourProxy)) {
    p->rename(label());
  }

  connect(data, SIGNAL(activeScalarsChanged()), SLOT(onScalarArrayChanged()));
  onScalarArrayChanged();

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

  updateGUI();
  onPropertyChanged();
}

void ModuleContour::createCategoricalColoringPipeline()
{
  if (m_pointDataToCellDataFilter == nullptr) {

    // Set up a point data to cell data filter and set the input data as
    // categorical
    vtkSMSourceProxy* producer = d->ColorByDataSource->proxy();

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
      .Set(d->ColorByDataSource->displayPosition(), 3);

    vtkSMPropertyHelper(m_pointDataToCellDataRepresentation, "Visibility")
      .Set(0);

    vtkSMPropertyHelper colorArrayHelper(
      m_pointDataToCellDataRepresentation, "ColorArrayName");
    d->ColorArrayName =
      std::string(colorArrayHelper.GetInputArrayNameToProcess());

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

  int colorByIndex = m_controllers->getColorByComboBox()->currentIndex();
  if (colorByIndex > 0) {
    createCategoricalColoringPipeline();
    auto childDataSources = getChildDataSources();
    d->ColorByDataSource = childDataSources[colorByIndex - 1];
    vtkSMPropertyHelper(m_resampleFilter, "CategoricalData").Set(1);
    vtkSMPropertyHelper(m_resampleRepresentation, "Visibility").Set(0);
    m_resampleRepresentation->UpdateProperty("Visibility");
    m_activeRepresentation = m_pointDataToCellDataRepresentation;
  } else {
    d->ColorByDataSource = dataSource();
    vtkSMPropertyHelper(m_resampleFilter, "CategoricalData").Set(0);
    if (m_pointDataToCellDataRepresentation) {
      vtkSMPropertyHelper(m_pointDataToCellDataRepresentation, "Visibility")
        .Set(0);
      m_pointDataToCellDataRepresentation->UpdateProperty("Visibility");
    }
    m_activeRepresentation = m_resampleRepresentation;
  }
  setVisibility(true);

  vtkSMPropertyHelper resampleHelper(m_resampleFilter, "Input");
  resampleHelper.Set(d->ColorByDataSource->proxy());

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

QJsonObject ModuleContour::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  vtkSMPropertyHelper contourValues(
    m_contourFilter->GetProperty("ContourValues"));
  props["contourValue"] = contourValues.GetAsDouble();
  props["useSolidColor"] = d->UseSolidColor;

  auto toJson = [](vtkSMProxy* representation) {
      QJsonObject obj;
      QJsonArray color;
      vtkSMPropertyHelper diffuseColor(
        representation->GetProperty("DiffuseColor"));
      for (int i = 0; i < 3; i++) {
        color.append(diffuseColor.GetAsDouble(i));
      }
      obj["color"] = color;

      QJsonObject lighting;
      vtkSMPropertyHelper ambient(representation->GetProperty("Ambient"));
      lighting["ambient"] = ambient.GetAsDouble();
      vtkSMPropertyHelper diffuse(representation->GetProperty("Diffuse"));
      lighting["diffuse"] = diffuse.GetAsDouble();
      vtkSMPropertyHelper specular(representation->GetProperty("Specular"));
      lighting["specular"] = specular.GetAsDouble();
      vtkSMPropertyHelper specularPower(
        representation->GetProperty("SpecularPower"));
      lighting["specularPower"] = specularPower.GetAsDouble();
      obj["lighting"] = lighting;

      vtkSMPropertyHelper representationHelper(
        representation->GetProperty("Representation"));
      obj["representation"] = representationHelper.GetAsString();

      vtkSMPropertyHelper opacity(representation->GetProperty("Opacity"));
      obj["opacity"] = opacity.GetAsDouble();

      vtkSMPropertyHelper mapScalars(representation->GetProperty("MapScalars"));
      obj["mapScalars"] = mapScalars.GetAsInt() == 1;

      return obj;
    };

  props["resampleRepresentation"] = toJson(m_resampleRepresentation);

  if (m_pointDataToCellDataRepresentation) {
    props["pointDataToCellDataRepresentation"] =
      toJson(m_pointDataToCellDataRepresentation);
  }

  json["properties"] = props;

  return json;
}

bool ModuleContour::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    if (m_contourFilter != nullptr) {
      vtkSMPropertyHelper(m_contourFilter, "ContourValues")
        .Set(props["contourValue"].toDouble());
      m_resampleFilter->UpdateVTKObjects();
    }

    d->UseSolidColor = props["useSolidColor"].toBool();
    m_controllers->setUseSolidColor(d->UseSolidColor);

    auto toRep = [](vtkSMProxy* representation, const QJsonObject& state) {
        QJsonObject lighting = state["lighting"].toObject();
        vtkSMPropertyHelper ambient(representation, "Ambient");
        ambient.Set(lighting["ambient"].toDouble());
        vtkSMPropertyHelper diffuse(representation, "Diffuse");
        diffuse.Set(lighting["diffuse"].toDouble());
        vtkSMPropertyHelper specular(representation, "Specular");
        specular.Set(lighting["specular"].toDouble());
        vtkSMPropertyHelper specularPower(representation, "SpecularPower");
        specularPower.Set(lighting["specularPower"].toDouble());
        auto color = state["color"].toArray();
        vtkSMPropertyHelper diffuseColor(representation, "DiffuseColor");
        for (int i = 0; i < 3; i++) {
          diffuseColor.Set(i, color[i].toDouble());
        }
        vtkSMPropertyHelper opacity(representation, "Opacity");
        opacity.Set(state["opacity"].toDouble());
        vtkSMPropertyHelper mapScalars(representation, "MapScalars");
        mapScalars.Set(state["mapScalars"].toBool() ? 1 : 0);
        representation->UpdateVTKObjects();
      };

    if (props.contains("resampleRepresentation")) {
      auto resampleRepresentationState =
        props["resampleRepresentation"].toObject();
      toRep(m_resampleRepresentation, resampleRepresentationState);
    }

    return true;
  }
  return false;
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
  return d->ColorByDataSource.data()
           ? d->ColorByDataSource.data()
           : dataSource();
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
  if (!d->ColorByDataSource) {
    return;
  }

  std::string arrayName(d->ColorArrayName);

  // Get the active point scalars from the resample filter
  vtkPVDataInformation* dataInfo = nullptr;
  vtkPVDataSetAttributesInformation* attributeInfo = nullptr;
  vtkPVArrayInformation* arrayInfo = nullptr;
  if (d->ColorByDataSource) {
    dataInfo = d->ColorByDataSource->proxy()->GetDataInformation(0);
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

  vtkSMPropertyHelper colorArrayHelper(m_activeRepresentation,
                                       "ColorArrayName");
  if (d->UseSolidColor) {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_POINTS, "");
  } else if (m_controllers &&
             m_controllers->getColorByComboBox()->currentIndex() > 0) {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_CELLS, arrayName.c_str());
  } else {
    colorArrayHelper.SetInputArrayToProcess(
      vtkDataObject::FIELD_ASSOCIATION_POINTS, arrayName.c_str());
  }

  ActiveObjects::instance().colorMapChanged(d->ColorByDataSource);
}

void ModuleContour::setUseSolidColor(const bool useSolidColor)
{
  d->UseSolidColor = useSolidColor;
  updateColorMap();
  emit renderNeeded();
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
    combo->addItem("This Data");
    for (int i = 0; i < childSources.size(); ++i) {
      combo->addItem(childSources[i]->fileName());
    }

    int selected = childSources.indexOf(d->ColorByDataSource);

    // If data source not found, selected will be -1, so the current index will
    // be set to 0, which is the right index for this data source.
    combo->setCurrentIndex(selected + 1);
    combo->blockSignals(false);
  }
}

} // end of namespace tomviz
