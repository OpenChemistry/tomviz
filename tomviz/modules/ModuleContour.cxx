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

#include "pqApplicationCore.h"
#include "pqColorChooserButton.h"
#include "pqPropertyLinks.h"
#include "pqSettings.h"
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
#include <cfloat>
#include <functional>
#include <string>
#include <vector>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLayout>
#include <QPointer>
#include <QSettings>

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
  vtkSMPropertyHelper(m_contourFilter, "ComputeScalars", /*quiet*/ true).Set(1);

  double contourStartVal = dataSource()->initialContourValue();

  // Check if this start value has been set by the outside (i. e., by double
  // clicking on the histogram). If not, then we will set it ourselves.
  if (contourStartVal == DBL_MAX) {
    // Get the range to calculate an initial value for the contour
    double range[2];
    dataSource()->getRange(range);

    // Use 2/3 of the range for the initial value of the contour
    contourStartVal = 2.0 / 3.0 * (range[1] - range[0]) + range[0];
  } else {
    // We will only use the initial contour value once. Reset it after
    // its first use.
    dataSource()->setInitialContourValue(DBL_MAX);
  }

  vtkSMPropertyHelper(m_contourFilter, "ContourValues").Set(contourStartVal);

  // Ask the user if they would like to change the initial value for the contour
  userSelectInitialContourValue();

  controller->PostInitializeProxy(m_contourFilter);
  controller->RegisterPipelineProxy(m_contourFilter);

  m_activeRepresentation = controller->Show(m_contourFilter, 0, vtkView);

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
  vtkSMPropertyHelper(m_activeRepresentation, "LookupTable").Set(colorMap());

  updateScalarColoring();

  vtkSMPropertyHelper(m_activeRepresentation, "Visibility")
    .Set(visibility() ? 1 : 0);
  m_activeRepresentation->UpdateVTKObjects();
}

bool ModuleContour::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_activeRepresentation);
  controller->UnRegisterProxy(m_contourFilter);
  m_activeRepresentation = nullptr;
  m_contourFilter = nullptr;
  return true;
}

bool ModuleContour::setVisibility(bool val)
{
  Q_ASSERT(m_activeRepresentation);
  vtkSMPropertyHelper(m_activeRepresentation, "Visibility").Set(val ? 1 : 0);
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

void ModuleContour::setIsoValue(double value)
{
  vtkSMPropertyHelper(m_contourFilter, "ContourValues").Set(value);
  m_contourFilter->UpdateVTKObjects();
}

double ModuleContour::getIsoValue() const
{
  return vtkSMPropertyHelper(m_contourFilter, "ContourValues").GetAsDouble();
}

void ModuleContour::addToPanel(QWidget* panel)
{
  Q_ASSERT(m_contourFilter);

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
  m_controllers->addPropertyLinks(d->Links, m_activeRepresentation,
                                  m_contourFilter);
  connect(m_controllers, SIGNAL(propertyChanged()), this,
          SLOT(onPropertyChanged()));

  onPropertyChanged();
}

void ModuleContour::onPropertyChanged()
{
  d->Links.accept();

  if (!m_controllers) {
    return;
  }

  d->ColorByDataSource = dataSource();
  setVisibility(true);

  updateColorMap();

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

  props["activeRepresentation"] = toJson(m_activeRepresentation);

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
      m_contourFilter->UpdateVTKObjects();
    }

    d->UseSolidColor = props["useSolidColor"].toBool();
    if (m_controllers) {
      m_controllers->setUseSolidColor(d->UseSolidColor);
    }

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
      vtkSMPropertyHelper representationHelper(representation,
                                               "Representation");
      representationHelper.Set(
        state["representation"].toString().toLocal8Bit().data());
      representation->UpdateVTKObjects();
    };

    if (props.contains("activeRepresentation")) {
      auto activeRepresentationState = props["activeRepresentation"].toObject();
      toRep(m_activeRepresentation, activeRepresentationState);
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
  return d->ColorByDataSource.data() ? d->ColorByDataSource.data()
                                     : dataSource();
}

bool ModuleContour::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return proxy == m_contourFilter.Get();
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
  } else {
    qWarning("Gave bad proxy to module in save animation state");
    return "";
  }
}

vtkSMProxy* ModuleContour::getProxyForString(const std::string& str)
{
  if (str == "Contour") {
    return m_contourFilter.Get();
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

void ModuleContour::userSelectInitialContourValue()
{
  QSettings* settings = pqApplicationCore::instance()->settings();
  bool userConfirmInitialValue =
    settings->value("ContourSettings.UserConfirmInitialValue", true).toBool();

  if (!userConfirmInitialValue)
    return;

  QDialog dialog;
  QVBoxLayout layout;
  dialog.setLayout(&layout);
  dialog.setWindowTitle(tr("Initial Contour Value"));

  // Get the range of the dataset
  double range[2];
  dataSource()->getRange(range);

  DoubleSliderWidget w(true);
  w.setMinimum(range[0]);
  w.setMaximum(range[1]);

  // We want to round this to two decimal places
  double isoValue = getIsoValue();
  isoValue = QString::number(isoValue, 'f', 2).toDouble();
  w.setValue(isoValue);

  w.setLineEditWidth(50);

  layout.addWidget(&w);

  QCheckBox dontAskAgain("Don't ask again");
  layout.addWidget(&dontAskAgain);
  layout.setAlignment(&dontAskAgain, Qt::AlignCenter);

  QDialogButtonBox ok(QDialogButtonBox::Ok);
  layout.addWidget(&ok);
  layout.setAlignment(&ok, Qt::AlignCenter);
  connect(&ok, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

  dialog.exec();

  if (dontAskAgain.isChecked())
    settings->setValue("ContourSettings.UserConfirmInitialValue", false);

  setIsoValue(w.value());
}

} // end of namespace tomviz
