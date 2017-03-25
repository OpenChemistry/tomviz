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
#include "ModuleOrthogonalSlice.h"

#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "IntSliderWidget.h"
#include "Utilities.h"
#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqWidgetRangeDomain.h"
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

namespace tomviz {

ModuleOrthogonalSlice::ModuleOrthogonalSlice(QObject* parentObject)
  : Module(parentObject)
{
}

ModuleOrthogonalSlice::~ModuleOrthogonalSlice()
{
  finalize();
}

QIcon ModuleOrthogonalSlice::icon() const
{
  return QIcon(":/icons/orthoslice.png");
}

bool ModuleOrthogonalSlice::initialize(DataSource* data,
                                       vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = data->producer()->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  m_passThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_passThrough);
  controller->PreInitializeProxy(m_passThrough);
  vtkSMPropertyHelper(m_passThrough, "Input").Set(data->producer());
  controller->PostInitializeProxy(m_passThrough);
  controller->RegisterPipelineProxy(m_passThrough);

  // Create the representation for it.
  m_representation = controller->Show(m_passThrough, 0, vtkView);
  Q_ASSERT(m_representation);

  vtkSMRepresentationProxy::SetRepresentationType(m_representation, "Slice");
  vtkSMPropertyHelper(m_representation, "Position")
    .Set(data->displayPosition(), 3);

  // pick proper color/opacity maps.
  updateColorMap();
  m_representation->UpdateVTKObjects();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  return true;
}

void ModuleOrthogonalSlice::updateColorMap()
{
  Q_ASSERT(m_representation);

  vtkSMPropertyHelper(m_representation, "LookupTable").Set(colorMap());
  vtkSMPropertyHelper(m_representation, "ScalarOpacityFunction")
    .Set(opacityMap());
  m_representation->UpdateVTKObjects();
}

bool ModuleOrthogonalSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_representation);
  controller->UnRegisterProxy(m_passThrough);

  m_passThrough = nullptr;
  m_representation = nullptr;
  return true;
}

bool ModuleOrthogonalSlice::setVisibility(bool val)
{
  Q_ASSERT(m_representation);
  vtkSMPropertyHelper(m_representation, "Visibility").Set(val ? 1 : 0);
  m_representation->UpdateVTKObjects();
  return true;
}

bool ModuleOrthogonalSlice::visibility() const
{
  if (m_representation) {
    return vtkSMPropertyHelper(m_representation, "Visibility").GetAsInt() != 0;
  } else {
    return false;
  }
}

void ModuleOrthogonalSlice::addToPanel(QWidget* panel)
{
  Q_ASSERT(m_representation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QFormLayout* layout = new QFormLayout;

  QComboBox* direction = new QComboBox;
  direction->addItem("XY Plane");
  direction->addItem("YZ Plane");
  direction->addItem("XZ Plane");

  layout->addRow("Direction", direction);

  pqSignalAdaptorComboBox* adaptor = new pqSignalAdaptorComboBox(direction);

  IntSliderWidget* sliceIndex = new IntSliderWidget(true);
  sliceIndex->setLineEditWidth(50);
  sliceIndex->setPageStep(1);
  layout->addRow("Slice", sliceIndex);

  DoubleSliderWidget* opacitySlider = new DoubleSliderWidget(true);
  opacitySlider->setLineEditWidth(50);
  layout->addRow("Opacity", opacitySlider);

  panel->setLayout(layout);

  m_links.addPropertyLink(sliceIndex, "value", SIGNAL(valueEdited(int)),
                          m_representation,
                          m_representation->GetProperty("Slice"), 0);
  new pqWidgetRangeDomain(sliceIndex, "minimum", "maximum",
                          m_representation->GetProperty("Slice"), 0);
  m_links.addPropertyLink(opacitySlider, "value", SIGNAL(valueEdited(double)),
                          m_representation,
                          m_representation->GetProperty("Opacity"), 0);

  m_links.addPropertyLink(adaptor, "currentText",
                          SIGNAL(currentTextChanged(QString)), m_representation,
                          m_representation->GetProperty("SliceMode"));

  connect(sliceIndex, &IntSliderWidget::valueEdited, this,
          &ModuleOrthogonalSlice::dataUpdated);
  connect(direction, &QComboBox::currentTextChanged, this,
          &ModuleOrthogonalSlice::dataUpdated);
  connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleOrthogonalSlice::dataUpdated);
}

void ModuleOrthogonalSlice::dataUpdated()
{
  m_links.accept();
  emit renderNeeded();
}

bool ModuleOrthogonalSlice::serialize(pugi::xml_node& ns) const
{
  QStringList reprProperties;
  reprProperties << "SliceMode"
                 << "Slice"
                 << "Opacity"
                 << "Visibility";
  pugi::xml_node nodeR = ns.append_child("Representation");
  return (tomviz::serialize(m_representation, nodeR, reprProperties) &&
          Module::serialize(ns));
}

bool ModuleOrthogonalSlice::deserialize(const pugi::xml_node& ns)
{
  if (!tomviz::deserialize(m_representation, ns.child("Representation"))) {
    return false;
  }
  return Module::deserialize(ns);
}

void ModuleOrthogonalSlice::dataSourceMoved(double newX, double newY,
                                            double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_representation, "Position").Set(pos, 3);
  m_representation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == m_passThrough.Get()) || (proxy == m_representation.Get());
}

std::string ModuleOrthogonalSlice::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_passThrough.Get()) {
    return "PassThrough";
  } else if (proxy == m_representation.Get()) {
    return "Representation";
  } else {
    qWarning(
      "Unknown proxy passed to module orthogonal slice in save animation");
    return "";
  }
}

vtkSMProxy* ModuleOrthogonalSlice::getProxyForString(const std::string& str)
{
  if (str == "PassThrough") {
    return m_passThrough.Get();
  } else if (str == "Representation") {
    return m_representation.Get();
  } else {
    return nullptr;
  }
}
}
