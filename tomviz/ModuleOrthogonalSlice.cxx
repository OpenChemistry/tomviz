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
  : Superclass(parentObject)
{
}

ModuleOrthogonalSlice::~ModuleOrthogonalSlice()
{
  this->finalize();
}

QIcon ModuleOrthogonalSlice::icon() const
{
  return QIcon(":/icons/orthoslice.png");
}

bool ModuleOrthogonalSlice::initialize(DataSource* data,
                                       vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = data->producer()->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  this->PassThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->PassThrough);
  controller->PreInitializeProxy(this->PassThrough);
  vtkSMPropertyHelper(this->PassThrough, "Input").Set(data->producer());
  controller->PostInitializeProxy(this->PassThrough);
  controller->RegisterPipelineProxy(this->PassThrough);

  // Create the representation for it.
  this->Representation = controller->Show(this->PassThrough, 0, vtkView);
  Q_ASSERT(this->Representation);

  vtkSMRepresentationProxy::SetRepresentationType(this->Representation,
                                                  "Slice");
  vtkSMPropertyHelper(this->Representation, "Position")
    .Set(data->displayPosition(), 3);

  // pick proper color/opacity maps.
  this->updateColorMap();
  this->Representation->UpdateVTKObjects();
  return true;
}

void ModuleOrthogonalSlice::updateColorMap()
{
  Q_ASSERT(this->Representation);

  vtkSMPropertyHelper(this->Representation, "LookupTable")
    .Set(this->colorMap());
  vtkSMPropertyHelper(this->Representation, "ScalarOpacityFunction")
    .Set(this->opacityMap());
  this->Representation->UpdateVTKObjects();
}

bool ModuleOrthogonalSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->Representation);
  controller->UnRegisterProxy(this->PassThrough);

  this->PassThrough = nullptr;
  this->Representation = nullptr;
  return true;
}

bool ModuleOrthogonalSlice::setVisibility(bool val)
{
  Q_ASSERT(this->Representation);
  vtkSMPropertyHelper(this->Representation, "Visibility").Set(val ? 1 : 0);
  this->Representation->UpdateVTKObjects();
  return true;
}

bool ModuleOrthogonalSlice::visibility() const
{
  Q_ASSERT(this->Representation);
  return vtkSMPropertyHelper(this->Representation, "Visibility").GetAsInt() !=
         0;
}

void ModuleOrthogonalSlice::addToPanel(QWidget* panel)
{
  Q_ASSERT(this->Representation);

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

  this->Links.addPropertyLink(sliceIndex, "value", SIGNAL(valueEdited(int)),
                              this->Representation,
                              this->Representation->GetProperty("Slice"), 0);
  new pqWidgetRangeDomain(sliceIndex, "minimum", "maximum",
                          this->Representation->GetProperty("Slice"), 0);
  this->Links.addPropertyLink(opacitySlider, "value",
                              SIGNAL(valueEdited(double)), this->Representation,
                              this->Representation->GetProperty("Opacity"), 0);

  this->Links.addPropertyLink(
    adaptor, "currentText", SIGNAL(currentTextChanged(QString)),
    this->Representation, this->Representation->GetProperty("SliceMode"));

  this->connect(sliceIndex, &IntSliderWidget::valueEdited, this,
                &ModuleOrthogonalSlice::dataUpdated);
  this->connect(direction, &QComboBox::currentTextChanged, this,
                &ModuleOrthogonalSlice::dataUpdated);
  this->connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
                &ModuleOrthogonalSlice::dataUpdated);
}

void ModuleOrthogonalSlice::dataUpdated()
{
  this->Links.accept();
  emit this->renderNeeded();
}

bool ModuleOrthogonalSlice::serialize(pugi::xml_node& ns) const
{
  QStringList reprProperties;
  reprProperties << "SliceMode"
                 << "Slice"
                 << "Opacity"
                 << "Visibility";
  pugi::xml_node nodeR = ns.append_child("Representation");
  return (tomviz::serialize(this->Representation, nodeR, reprProperties) &&
          this->Superclass::serialize(ns));
}

bool ModuleOrthogonalSlice::deserialize(const pugi::xml_node& ns)
{
  if (!tomviz::deserialize(this->Representation, ns.child("Representation"))) {
    return false;
  }
  return this->Superclass::deserialize(ns);
}

void ModuleOrthogonalSlice::dataSourceMoved(double newX, double newY,
                                            double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(this->Representation, "Position").Set(pos, 3);
  this->Representation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == this->PassThrough.Get()) ||
         (proxy == this->Representation.Get());
}

std::string ModuleOrthogonalSlice::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == this->PassThrough.Get()) {
    return "PassThrough";
  } else if (proxy == this->Representation.Get()) {
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
    return this->PassThrough.Get();
  } else if (str == "Representation") {
    return this->Representation.Get();
  } else {
    return nullptr;
  }
}
}
