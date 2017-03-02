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

#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
namespace tomviz {

ModuleThreshold::ModuleThreshold(QObject* parentObject)
  : Superclass(parentObject)
{
}

ModuleThreshold::~ModuleThreshold()
{
  this->finalize();
}

QIcon ModuleThreshold::icon() const
{
  return QIcon(":/icons/pqThreshold.png");
}

bool ModuleThreshold::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView)) {
    return false;
  }

  vtkSMSourceProxy* producer = data->producer();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  // Create the contour filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "Threshold"));

  this->ThresholdFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->ThresholdFilter);
  controller->PreInitializeProxy(this->ThresholdFilter);
  vtkSMPropertyHelper(this->ThresholdFilter, "Input").Set(producer);
  controller->PostInitializeProxy(this->ThresholdFilter);
  controller->RegisterPipelineProxy(this->ThresholdFilter);

  // Update min/max to avoid thresholding the full dataset.
  vtkSMPropertyHelper rangeProperty(this->ThresholdFilter, "ThresholdBetween");
  double range[2], newRange[2];
  rangeProperty.Get(range, 2);
  double delta = (range[1] - range[0]);
  double mid = ((range[0] + range[1]) / 2.0);
  newRange[0] = mid - 0.1 * delta;
  newRange[1] = mid + 0.1 * delta;
  rangeProperty.Set(newRange, 2);
  this->ThresholdFilter->UpdateVTKObjects();

  // Create the representation for it.
  this->ThresholdRepresentation =
    controller->Show(this->ThresholdFilter, 0, vtkView);
  Q_ASSERT(this->ThresholdRepresentation);
  vtkSMRepresentationProxy::SetRepresentationType(this->ThresholdRepresentation,
                                                  "Surface");
  vtkSMPropertyHelper(this->ThresholdRepresentation, "Position")
    .Set(data->displayPosition(), 3);
  this->updateColorMap();
  this->ThresholdRepresentation->UpdateVTKObjects();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  return true;
}

void ModuleThreshold::updateColorMap()
{
  Q_ASSERT(this->ThresholdRepresentation);

  // by default, use the data source's color/opacity maps.
  vtkSMPropertyHelper(this->ThresholdRepresentation, "LookupTable")
    .Set(this->colorMap());
  vtkSMPropertyHelper(this->ThresholdRepresentation, "ScalarOpacityFunction")
    .Set(this->opacityMap());

  this->ThresholdRepresentation->UpdateVTKObjects();
}

bool ModuleThreshold::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ThresholdRepresentation);
  controller->UnRegisterProxy(this->ThresholdFilter);
  this->ThresholdFilter = nullptr;
  this->ThresholdRepresentation = nullptr;
  return true;
}

bool ModuleThreshold::setVisibility(bool val)
{
  Q_ASSERT(this->ThresholdRepresentation);
  vtkSMPropertyHelper(this->ThresholdRepresentation, "Visibility")
    .Set(val ? 1 : 0);
  this->ThresholdRepresentation->UpdateVTKObjects();
  return true;
}

bool ModuleThreshold::visibility() const
{
  if (this->ThresholdRepresentation) {
    return vtkSMPropertyHelper(this->ThresholdRepresentation, "Visibility")
             .GetAsInt() != 0;
  } else {
    return false;
  }
}

void ModuleThreshold::addToPanel(QWidget* panel)
{
  Q_ASSERT(this->ThresholdFilter);
  Q_ASSERT(this->ThresholdRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;

  pqStringVectorPropertyWidget* arraySelection =
    new pqStringVectorPropertyWidget(
      this->ThresholdFilter->GetProperty("SelectInputScalars"),
      this->ThresholdFilter.Get());
  layout->addWidget(arraySelection);

  pqDoubleRangeSliderPropertyWidget* range =
    new pqDoubleRangeSliderPropertyWidget(
      this->ThresholdFilter.Get(),
      this->ThresholdFilter->GetProperty("ThresholdBetween"));
  range->setProperty(this->ThresholdFilter->GetProperty("ThresholdBetween"));
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

  layout->addStretch();
  panel->setLayout(layout);

  pqSignalAdaptorComboBox* adaptor =
    new pqSignalAdaptorComboBox(representations);

  m_links.addPropertyLink(
    adaptor, "currentText", SIGNAL(currentTextChanged(QString)),
    this->ThresholdRepresentation,
    this->ThresholdRepresentation->GetProperty("Representation"));

  m_links.addPropertyLink(opacitySlider, "value", SIGNAL(valueEdited(double)),
                          this->ThresholdRepresentation,
                          this->ThresholdRepresentation->GetProperty("Opacity"),
                          0);
  m_links.addPropertyLink(
    specularSlider, "value", SIGNAL(valueEdited(double)),
    this->ThresholdRepresentation,
    this->ThresholdRepresentation->GetProperty("Specular"), 0);

  this->connect(arraySelection, &pqPropertyWidget::changeFinished,
                arraySelection, &pqPropertyWidget::apply);
  this->connect(arraySelection, &pqPropertyWidget::changeFinished, this,
                &Module::renderNeeded);
  this->connect(range, &pqPropertyWidget::changeFinished, range,
                &pqPropertyWidget::apply);
  this->connect(range, &pqPropertyWidget::changeFinished, this,
                &Module::renderNeeded);
  this->connect(representations, &QComboBox::currentTextChanged, this,
                &ModuleThreshold::dataUpdated);
  this->connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleThreshold::dataUpdated);
  this->connect(specularSlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleThreshold::dataUpdated);
}

void ModuleThreshold::dataUpdated()
{
  m_links.accept();
  emit this->renderNeeded();
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
                           << "Visibility";
  pugi::xml_node rnode = ns.append_child("ThresholdRepresentation");
  return tomviz::serialize(this->ThresholdFilter, tnode, fprops) &&
         tomviz::serialize(this->ThresholdRepresentation, rnode,
                           representationProperties) &&
         this->Superclass::serialize(ns);
}

bool ModuleThreshold::deserialize(const pugi::xml_node& ns)
{
  return tomviz::deserialize(this->ThresholdFilter, ns.child("Threshold")) &&
         tomviz::deserialize(this->ThresholdRepresentation,
                             ns.child("ThresholdRepresentation")) &&
         this->Superclass::deserialize(ns);
}

void ModuleThreshold::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(this->ThresholdRepresentation, "Position").Set(pos, 3);
  this->ThresholdRepresentation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleThreshold::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == this->ThresholdFilter.Get()) ||
         (proxy == this->ThresholdRepresentation.Get());
}

std::string ModuleThreshold::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == this->ThresholdFilter.Get()) {
    return "Threshold";
  } else if (proxy == this->ThresholdRepresentation.Get()) {
    return "Representation";
  } else {
    qWarning("Unknown proxy passed to module threshold in save animation");
    return "";
  }
}

vtkSMProxy* ModuleThreshold::getProxyForString(const std::string& str)
{
  if (str == "Threshold") {
    return this->ThresholdFilter.Get();
  } else if (str == "Representation") {
    return this->ThresholdRepresentation.Get();
  } else {
    return nullptr;
  }
}
}
