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
#include "ModuleThreshold.h"

#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "Utilities.h"
#include "pqDoubleRangeSliderPropertyWidget.h"
#include "pqProxiesWidget.h"
#include "pqSignalAdaptors.h"
#include "pqStringVectorPropertyWidget.h"
#include "vtkNew.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
namespace tomviz {

ModuleThreshold::ModuleThreshold(QObject* parentObject)
  : Module(parentObject)
{
}

ModuleThreshold::~ModuleThreshold()
{
  finalize();
}

QIcon ModuleThreshold::icon() const
{
  return QIcon(":/icons/pqThreshold.png");
}

bool ModuleThreshold::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  auto producer = data->dataSourceProxy();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  // Create the contour filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "Threshold"));

  m_thresholdFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_thresholdFilter);
  controller->PreInitializeProxy(m_thresholdFilter);
  vtkSMPropertyHelper(m_thresholdFilter, "Input").Set(producer);
  controller->PostInitializeProxy(m_thresholdFilter);
  controller->RegisterPipelineProxy(m_thresholdFilter);

  // Update min/max to avoid thresholding the full dataset.
  vtkSMPropertyHelper rangeProperty(m_thresholdFilter, "ThresholdBetween");
  double range[2], newRange[2];
  rangeProperty.Get(range, 2);
  double delta = (range[1] - range[0]);
  double mid = ((range[0] + range[1]) / 2.0);
  newRange[0] = mid - 0.1 * delta;
  newRange[1] = mid + 0.1 * delta;
  rangeProperty.Set(newRange, 2);
  m_thresholdFilter->UpdateVTKObjects();

  // Create the representation for it.
  m_thresholdRepresentation =
    controller->Show(m_thresholdFilter, 0, vtkView);
  Q_ASSERT(m_thresholdRepresentation);
  vtkSMRepresentationProxy::SetRepresentationType(m_thresholdRepresentation,
                                                  "Surface");
  vtkSMPropertyHelper(m_thresholdRepresentation, "Position")
    .Set(data->displayPosition(), 3);
  updateColorMap();
  m_thresholdRepresentation->UpdateVTKObjects();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  return true;
}

void ModuleThreshold::updateColorMap()
{
  Q_ASSERT(m_thresholdRepresentation);

  // by default, use the data source's color/opacity maps.
  vtkSMPropertyHelper(m_thresholdRepresentation, "LookupTable")
    .Set(colorMap());
  vtkSMPropertyHelper(m_thresholdRepresentation, "ScalarOpacityFunction")
    .Set(opacityMap());

  m_thresholdRepresentation->UpdateVTKObjects();
}

bool ModuleThreshold::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_thresholdRepresentation);
  controller->UnRegisterProxy(m_thresholdFilter);
  m_thresholdFilter = nullptr;
  m_thresholdRepresentation = nullptr;
  return true;
}

bool ModuleThreshold::setVisibility(bool val)
{
  Q_ASSERT(m_thresholdRepresentation);
  vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility")
    .Set(val ? 1 : 0);
  m_thresholdRepresentation->UpdateVTKObjects();
  return true;
}

bool ModuleThreshold::visibility() const
{
  if (m_thresholdRepresentation) {
    return vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility")
             .GetAsInt() != 0;
  } else {
    return false;
  }
}

void ModuleThreshold::addToPanel(QWidget* panel)
{
  Q_ASSERT(m_thresholdFilter);
  Q_ASSERT(m_thresholdRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;

  pqStringVectorPropertyWidget* arraySelection =
    new pqStringVectorPropertyWidget(
      m_thresholdFilter->GetProperty("SelectInputScalars"),
      m_thresholdFilter.Get());
  layout->addWidget(arraySelection);

  pqDoubleRangeSliderPropertyWidget* range =
    new pqDoubleRangeSliderPropertyWidget(
      m_thresholdFilter.Get(),
      m_thresholdFilter->GetProperty("ThresholdBetween"));
  range->setProperty(m_thresholdFilter->GetProperty("ThresholdBetween"));
  layout->addWidget(range);

  QFormLayout* formLayout = new QFormLayout;
  formLayout->setHorizontalSpacing(5);
  layout->addItem(formLayout);

  QComboBox* representations = new QComboBox;
  representations->addItem("Surface");
  representations->addItem("Wireframe");
  representations->addItem("Points");
  formLayout->addRow("Representation", representations);

  DoubleSliderWidget* opacitySlider = new DoubleSliderWidget(true);
  opacitySlider->setLineEditWidth(50);
  formLayout->addRow("Opacity", opacitySlider);

  DoubleSliderWidget* specularSlider = new DoubleSliderWidget(true);
  specularSlider->setLineEditWidth(50);
  formLayout->addRow("Specular", specularSlider);

  QCheckBox* mapScalarsCheckBox = new QCheckBox();
  formLayout->addRow("Color Map Data", mapScalarsCheckBox);

  layout->addStretch();
  panel->setLayout(layout);

  pqSignalAdaptorComboBox* adaptor =
    new pqSignalAdaptorComboBox(representations);

  m_links.addPropertyLink(
    adaptor, "currentText", SIGNAL(currentTextChanged(QString)),
    m_thresholdRepresentation,
    m_thresholdRepresentation->GetProperty("Representation"));

  m_links.addPropertyLink(opacitySlider, "value", SIGNAL(valueEdited(double)),
                          m_thresholdRepresentation,
                          m_thresholdRepresentation->GetProperty("Opacity"),
                          0);
  m_links.addPropertyLink(
    specularSlider, "value", SIGNAL(valueEdited(double)),
    m_thresholdRepresentation,
    m_thresholdRepresentation->GetProperty("Specular"), 0);

  m_links.addPropertyLink(mapScalarsCheckBox, "checked", SIGNAL(toggled(bool)),
                          m_thresholdRepresentation,
                          m_thresholdRepresentation->GetProperty("MapScalars"),
                          0);

  connect(arraySelection, &pqPropertyWidget::changeFinished,
                arraySelection, &pqPropertyWidget::apply);
  connect(arraySelection, &pqPropertyWidget::changeFinished, this,
                &Module::renderNeeded);
  connect(range, &pqPropertyWidget::changeFinished, range,
                &pqPropertyWidget::apply);
  connect(range, &pqPropertyWidget::changeFinished, this,
                &Module::renderNeeded);
  connect(representations, &QComboBox::currentTextChanged, this,
                &ModuleThreshold::dataUpdated);
  connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleThreshold::dataUpdated);
  connect(specularSlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleThreshold::dataUpdated);
  connect(mapScalarsCheckBox, &QCheckBox::toggled, this,
          &ModuleThreshold::dataUpdated);
}

void ModuleThreshold::dataUpdated()
{
  m_links.accept();
  emit renderNeeded();
}

bool ModuleThreshold::serialize(pugi::xml_node& ns) const
{
  QStringList fprops;
  fprops << "SelectInputScalars"
         << "ThresholdBetween";
  pugi::xml_node tnode = ns.append_child("Threshold");

  QStringList representationProperties;
  representationProperties << "Representation"
                           << "Opacity"
                           << "Specular"
                           << "Visibility"
                           << "MapScalars";
  pugi::xml_node rnode = ns.append_child("ThresholdRepresentation");
  return tomviz::serialize(m_thresholdFilter, tnode, fprops) &&
         tomviz::serialize(m_thresholdRepresentation, rnode,
                           representationProperties) &&
         Module::serialize(ns);
}

bool ModuleThreshold::deserialize(const pugi::xml_node& ns)
{
  return tomviz::deserialize(m_thresholdFilter, ns.child("Threshold")) &&
         tomviz::deserialize(m_thresholdRepresentation,
                             ns.child("ThresholdRepresentation")) &&
         Module::deserialize(ns);
}

void ModuleThreshold::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_thresholdRepresentation, "Position").Set(pos, 3);
  m_thresholdRepresentation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleThreshold::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == m_thresholdFilter.Get()) ||
         (proxy == m_thresholdRepresentation.Get());
}

std::string ModuleThreshold::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_thresholdFilter.Get()) {
    return "Threshold";
  } else if (proxy == m_thresholdRepresentation.Get()) {
    return "Representation";
  } else {
    qWarning("Unknown proxy passed to module threshold in save animation");
    return "";
  }
}

vtkSMProxy* ModuleThreshold::getProxyForString(const std::string& str)
{
  if (str == "Threshold") {
    return m_thresholdFilter.Get();
  } else if (str == "Representation") {
    return m_thresholdRepresentation.Get();
  } else {
    return nullptr;
  }
}
}
