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
#include "ModuleMeasurementCube.h"
#include "ModuleMeasurementCubeWidget.h"

#include "DataSource.h"
#include "Utilities.h"

#include <vtkHandleWidget.h>
#include <vtkMeasurementCubeHandleRepresentation3D.h>

#include <pqCoreUtilities.h>
#include <pqProxiesWidget.h>
#include <vtkCommand.h>
#include <vtkPVDataInformation.h>
#include <vtkPVRenderView.h>
#include <vtkSMPVRepresentationProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

namespace tomviz {

using pugi::xml_attribute;
using pugi::xml_node;

ModuleMeasurementCube::ModuleMeasurementCube(QObject* parentObject) :
  Superclass(parentObject)
{
  // Connect to m_cubeRep's "modified" signal, and emit it as our own
  // "onPositionChanged" signal
  this->m_observedPositionId = pqCoreUtilities::connect(
    m_cubeRep.Get(), vtkCommand::ModifiedEvent, this,
    SIGNAL(onPositionChanged()));

  // Connect to our "onPositionChanged" signal and emit it with arguments
  connect(this,
	  (void (ModuleMeasurementCube::*)())(&ModuleMeasurementCube::onPositionChanged),
	  this,
	  [&](){
	    double p[3];
	    m_cubeRep->GetWorldPosition(p);
	    onPositionChanged(p[0], p[1], p[2]);
	  });

  // Connect to m_cubeRep's "modified" signal, and emit it as our own
  // "onSideLengthChanged" signal
  this->m_observedSideLengthId = pqCoreUtilities::connect(
    m_cubeRep.Get(), vtkCommand::ModifiedEvent, this,
    SIGNAL(onSideLengthChanged()));

  // Connect to our "onSideLengthChanged" signal and emit it with arguments
  connect(this,
	  (void (ModuleMeasurementCube::*)())(&ModuleMeasurementCube::onSideLengthChanged),
	  this,
	  [&](){
	    onSideLengthChanged(m_cubeRep->GetSideLength());
	  });
}

ModuleMeasurementCube::~ModuleMeasurementCube()
{
  m_cubeRep->RemoveObserver(m_observedPositionId);
  m_cubeRep->RemoveObserver(m_observedSideLengthId);

  this->finalize();
}

QIcon ModuleMeasurementCube::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqElemMapData16.png");
}

bool ModuleMeasurementCube::initialize(DataSource* data,
                                       vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView)) {
    return false;
  }

  connect(data, SIGNAL(dataPropertiesChanged()),
	  this, SLOT(dataPropertiesChanged()));

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_handleWidget->SetInteractor(m_view->GetInteractor());

  double bounds[6];
  data->producer()->GetDataInformation()->GetBounds(bounds);
  double length = std::max(floor((bounds[1]-bounds[0])*.1), 1.);
  double minPosition[3] = {bounds[0] + length*.5, bounds[2] + length*.5,
			   bounds[4] + length*.5};
  m_cubeRep->SetSideLength(length);
  m_cubeRep->PlaceWidget(minPosition);
  m_cubeRep->SetWorldPosition(minPosition);
  m_cubeRep->SetAdaptiveScaling(0);
  m_cubeRep->SetLengthUnit(data->getUnits(0).toStdString().c_str());

  m_handleWidget->SetRepresentation(m_cubeRep.Get());
  m_handleWidget->EnabledOn();
  return true;
}

bool ModuleMeasurementCube::finalize()
{
  return true;
}

bool ModuleMeasurementCube::visibility() const
{
  return m_cubeRep->GetHandleVisibility() == 1;
}

bool ModuleMeasurementCube::setVisibility(bool choice)
{
  m_cubeRep->SetHandleVisibility(choice ? 1 : 0);
  m_cubeRep->SetLabelVisibility(choice ? 1 : 0);
  return true;
}

bool ModuleMeasurementCube::serialize(pugi::xml_node& ns) const
{
  xml_node rootNode = ns.append_child("properties");

  xml_node sideLengthNode = rootNode.append_child("sideLength");
  sideLengthNode.append_attribute("value") = m_cubeRep->GetSideLength();

  double p[3];
  m_cubeRep->GetWorldPosition(p);
  xml_node positionNode = rootNode.append_child("position");
  positionNode.append_attribute("x") = p[0];
  positionNode.append_attribute("y") = p[1];
  positionNode.append_attribute("z") = p[2];

  xml_node visibilityNode = rootNode.append_child("visibility");
  visibilityNode.append_attribute("enabled") =
    m_cubeRep->GetHandleVisibility() == 1;

  xml_node adaptiveScalingNode = rootNode.append_child("adaptiveScaling");
  adaptiveScalingNode.append_attribute("enabled") =
    m_cubeRep->GetAdaptiveScaling() == 1;

  return Module::serialize(ns);
}

bool ModuleMeasurementCube::deserialize(const pugi::xml_node& ns)
{
  xml_node rootNode = ns.child("properties");
  if (!rootNode) {
    return false;
  }

  xml_node node = rootNode.child("sideLength");
  if (node) {
    xml_attribute att = node.attribute("value");
    if (att)
      m_cubeRep->SetSideLength(att.as_double());
  }

  node = rootNode.child("position");
  if (node) {
    double p[3];
    p[0] = node.attribute("x").as_double();
    p[1] = node.attribute("y").as_double();
    p[2] = node.attribute("z").as_double();
    m_cubeRep->SetWorldPosition(p);
  }

  node = rootNode.child("visibility");
  if (node) {
    xml_attribute att = node.attribute("enabled");
    if (att) {
      m_cubeRep->SetHandleVisibility(static_cast<int>(att.as_bool()));
    }
  }
  node = rootNode.child("adaptiveScaling");
  if (node) {
    xml_attribute att = node.attribute("enabled");
    if (att) {
      m_cubeRep->SetAdaptiveScaling(static_cast<int>(att.as_bool()));
    }
  }

  return Module::deserialize(ns);
}

void ModuleMeasurementCube::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  m_controllers = new ModuleMeasurementCubeWidget;
  layout->addWidget(m_controllers);

  // Set initial parameters
  m_controllers->setAdaptiveScaling(
    static_cast<bool>(m_cubeRep->GetAdaptiveScaling()));
  m_controllers->setSideLength(m_cubeRep->GetSideLength());
  m_controllers->setLengthUnit(QString(m_cubeRep->GetLengthUnit()));
  double worldPosition[3];
  m_cubeRep->GetWorldPosition(worldPosition);
  m_controllers->setPosition(worldPosition[0], worldPosition[1],
			     worldPosition[2]);
  m_controllers->setPositionUnit(QString(m_cubeRep->GetLengthUnit()));

  // Connect the widget's signals to this class' slots
  connect(m_controllers, SIGNAL(adaptiveScalingToggled(const bool)), this,
          SLOT(setAdaptiveScaling(const bool)));
  connect(m_controllers, SIGNAL(sideLengthChanged(const double)), this,
          SLOT(setSideLength(const double)));

  // Connect this class' signals to the widget's slots
  connect(this, SIGNAL(onLengthUnitChanged(const QString)),
	  m_controllers, SLOT(setLengthUnit(const QString)));
  connect(this, SIGNAL(onPositionUnitChanged(const QString)),
	  m_controllers, SLOT(setPositionUnit(const QString)));
  connect(this, SIGNAL(onSideLengthChanged(const double)),
	  m_controllers, SLOT(setSideLength(const double)));
  connect(this,
	  SIGNAL(onPositionChanged(const double, const double, const double)),
	  m_controllers,
	  SLOT(setPosition(const double, const double, const double)));
}

void ModuleMeasurementCube::setAdaptiveScaling(const bool val)
{
  m_cubeRep->SetAdaptiveScaling(val ? 1 : 0);
}

void ModuleMeasurementCube::setSideLength(const double length)
{
  m_cubeRep->SetSideLength(length);
}

void ModuleMeasurementCube::setLengthUnit()
{
  QString s = qobject_cast<DataSource*>(sender())->getUnits(0);
  m_cubeRep->SetLengthUnit(s.toStdString().c_str());
  emit onLengthUnitChanged(s);
}

void ModuleMeasurementCube::setPositionUnit()
{
  QString s = qobject_cast<DataSource*>(sender())->getUnits(0);
  emit onLengthUnitChanged(s);
}

void ModuleMeasurementCube::dataPropertiesChanged()
{
  DataSource* data = qobject_cast<DataSource*>(sender());
  if (!data) {
    return;
  }
  m_cubeRep->SetLengthUnit(data->getUnits(0).toStdString().c_str());

  emit onLengthUnitChanged(data->getUnits(0));
  emit onPositionUnitChanged(data->getUnits(0));
}

bool ModuleMeasurementCube::isProxyPartOfModule(vtkSMProxy*)
{
  return false;
}

std::string ModuleMeasurementCube::getStringForProxy(vtkSMProxy*)
{
  qWarning("Unknown proxy passed to module volume in save animation");
  return "";
}

vtkSMProxy* ModuleMeasurementCube::getProxyForString(const std::string&)
{
  return nullptr;
}

} // end of namespace tomviz
