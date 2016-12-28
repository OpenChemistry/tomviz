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

#include "ModuleRuler.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "Utilities.h"

#include <pqLinePropertyWidget.h>
#include <pqView.h>
#include <vtkAlgorithm.h>
#include <vtkDataSet.h>
#include <vtkNew.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <QHBoxLayout>
#include <QWidget>

namespace tomviz {

ModuleRuler::ModuleRuler(QObject* p) : Superclass(p)
{
}

ModuleRuler::~ModuleRuler()
{
  this->finalize();
}

QIcon ModuleRuler::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqRuler16.png");
}

bool ModuleRuler::initialize(DataSource* data, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(data, view)) {
    return false;
  }
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = data->producer()->GetSessionProxyManager();
  vtkAlgorithm* alg =
    vtkAlgorithm::SafeDownCast(data->producer()->GetClientSideObject());
  double bounds[6];
  vtkDataSet::SafeDownCast(alg->GetOutputDataObject(0))->GetBounds(bounds);
  double bounds_min[3] = { bounds[0], bounds[2], bounds[4] };
  double bounds_max[3] = { bounds[1], bounds[3], bounds[5] };

  m_RulerSource.TakeReference(
    vtkSMSourceProxy::SafeDownCast(pxm->NewProxy("sources", "Ruler")));
  vtkSMPropertyHelper(m_RulerSource, "Point1").Set(bounds_min, 3);
  vtkSMPropertyHelper(m_RulerSource, "Point2").Set(bounds_max, 3);
  m_RulerSource->UpdateVTKObjects();
  controller->RegisterPipelineProxy(m_RulerSource);

  m_Representation = controller->Show(m_RulerSource, 0, view);

  m_Representation->UpdateVTKObjects();

  return m_Representation && m_RulerSource;
}

bool ModuleRuler::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_Representation);
  controller->UnRegisterProxy(m_RulerSource);
  this->m_Representation = nullptr;
  this->m_RulerSource = nullptr;
  return true;
}

void ModuleRuler::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }
  QHBoxLayout* layout = new QHBoxLayout;

  pqLinePropertyWidget* widget = new pqLinePropertyWidget(
    m_RulerSource, m_RulerSource->GetPropertyGroup(0), panel);
  layout->addWidget(widget);
  widget->setView(
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView()));
  widget->select();
  layout->addStretch();
  QObject::connect(widget, &pqPropertyWidget::changeFinished, widget,
                   &pqPropertyWidget::apply);
  QObject::connect(widget, &pqPropertyWidget::changeFinished, this,
                   &Module::renderNeeded);
  panel->setLayout(layout);
}

bool ModuleRuler::setVisibility(bool val)
{
  vtkSMPropertyHelper(m_Representation, "Visibility").Set(val ? 1 : 0);
  m_Representation->UpdateVTKObjects();
  return true;
}

bool ModuleRuler::visibility() const
{
  return vtkSMPropertyHelper(m_Representation, "Visibility").GetAsInt() != 0;
}

bool ModuleRuler::serialize(pugi::xml_node& ns) const
{
  pugi::xml_node rulerNode = ns.append_child("Ruler");
  pugi::xml_node representationNode = ns.append_child("Representation");

  QStringList rulerProperties;
  rulerProperties << "Point1"
                  << "Point2";
  QStringList representationProperties;
  representationProperties << "Visibility";
  if (!tomviz::serialize(m_RulerSource, rulerNode, rulerProperties)) {
    qWarning("Failed to serialize ruler");
    return false;
  }
  if (!tomviz::serialize(m_Representation, representationNode,
                         representationProperties)) {
    qWarning("Failed to serialize ruler representation");
    return false;
  }
  return true;
}

bool ModuleRuler::deserialize(const pugi::xml_node& ns)
{
  return tomviz::deserialize(m_RulerSource, ns.child("Ruler")) &&
         tomviz::deserialize(m_Representation, ns.child("Representation"));
}

bool ModuleRuler::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return proxy == m_RulerSource.GetPointer() ||
         proxy == m_Representation.GetPointer();
}

std::string ModuleRuler::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_RulerSource.GetPointer()) {
    return "Ruler";
  } else if (proxy == m_Representation.GetPointer()) {
    return "Representation";
  } else {
    qWarning("Unknown proxy passed to module ruler in save animation");
    return "";
  }
}

vtkSMProxy* ModuleRuler::getProxyForString(const std::string& str)
{
  if (str == "Ruler") {
    return m_RulerSource;
  } else if (str == "Representation") {
    return m_Representation;
  } else {
    return nullptr;
  }
}
}
