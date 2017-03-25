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
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkRulerSourceRepresentation.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace tomviz {

ModuleRuler::ModuleRuler(QObject* p) : Superclass(p), m_showLine(true)
{
}

ModuleRuler::~ModuleRuler()
{
  this->finalize();
}

QIcon ModuleRuler::icon() const
{
  return QIcon(":/icons/pqRuler.png");
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

  updateUnits();

  QObject::connect(data, &DataSource::dataChanged, this,
                   &ModuleRuler::updateUnits);

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
  QVBoxLayout* layout = new QVBoxLayout;

  m_Widget = new pqLinePropertyWidget(
    m_RulerSource, m_RulerSource->GetPropertyGroup(0), panel);
  layout->addWidget(m_Widget);
  m_Widget->setView(
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView()));
  m_Widget->select();
  m_Widget->setWidgetVisible(m_showLine);
  layout->addStretch();
  QObject::connect(m_Widget.data(), &pqPropertyWidget::changeFinished, m_Widget.data(),
                   &pqPropertyWidget::apply);
  QObject::connect(m_Widget.data(), &pqPropertyWidget::changeFinished, this,
                   &ModuleRuler::endPointsUpdated);
  QObject::connect(m_Widget, SIGNAL(widgetVisibilityUpdated(bool)), this,
                   SLOT(updateShowLine(bool)));


  m_Widget->setWidgetVisible(m_showLine);

  QLabel* label0 = new QLabel("Point 0 data value: ");
  QLabel* label1 = new QLabel("Point 1 data value: ");
  QObject::connect(
    this, &ModuleRuler::newEndpointData, label0,
    [label0, label1](double val0, double val1) {
      label0->setText(QString("Point 0 data value: %1").arg(val0));
      label1->setText(QString("Point 1 data value: %1").arg(val1));
    });
  layout->addWidget(label0);
  layout->addWidget(label1);
  panel->setLayout(layout);
}

void ModuleRuler::prepareToRemoveFromPanel(QWidget* vtkNotUsed(panel))
{
  // Disconnect before the panel is removed to avoid m_showLine always being set
  // to false when the signal widgetVisibilityUpdated(bool) is emitted during
  // the tear down of the pqLinePropertyWidget.
  QObject::disconnect(m_Widget, SIGNAL(widgetVisibilityUpdated(bool)),
                      this, SLOT(updateShowLine(bool)));
}

bool ModuleRuler::setVisibility(bool val)
{
  vtkSMPropertyHelper(m_Representation, "Visibility").Set(val ? 1 : 0);
  m_Representation->UpdateVTKObjects();
  if (!val || m_showLine) {
    bool oldValue = m_showLine;
    m_Widget->setWidgetVisible(val);
    m_showLine = oldValue;
  }
  return true;
}

bool ModuleRuler::visibility() const
{
  if (m_Representation) {
    return vtkSMPropertyHelper(m_Representation, "Visibility").GetAsInt() != 0;
  } else {
    return false;
  }
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

  pugi::xml_node showLine = representationNode.append_child("ShowLine");
  showLine.append_attribute("value").set_value(m_showLine);

  if (!tomviz::serialize(m_Representation, representationNode,
                         representationProperties)) {
    qWarning("Failed to serialize ruler representation");
    return false;
  }

  return true;
}

bool ModuleRuler::deserialize(const pugi::xml_node& ns)
{
  pugi::xml_node representationNode = ns.child("Representation");
  bool success = tomviz::deserialize(m_RulerSource, ns.child("Ruler")) &&
    tomviz::deserialize(m_Representation, representationNode);

  if (representationNode) {
    pugi::xml_node showLineNode = representationNode.child("ShowLine");
    if (showLineNode) {
      pugi::xml_attribute valueAttribute = showLineNode.attribute("value");
      if (valueAttribute) {
        m_showLine = valueAttribute.as_bool();
      }
    }
  }

  return success;
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

void ModuleRuler::updateUnits()
{
  DataSource* source = dataSource();
  QString units = source->getUnits(0);
  vtkRulerSourceRepresentation* rep =
    vtkRulerSourceRepresentation::SafeDownCast(
      m_Representation->GetClientSideObject());
  QString labelFormat = "%-#6.3g %1";
  rep->SetLabelFormat(labelFormat.arg(units).toLatin1().data());
}

void ModuleRuler::updateShowLine(bool show)
{
  m_showLine = show;
}

void ModuleRuler::endPointsUpdated()
{
  double point1[3];
  double point2[3];
  vtkSMPropertyHelper(m_RulerSource, "Point1").Get(point1, 3);
  vtkSMPropertyHelper(m_RulerSource, "Point2").Get(point2, 3);
  DataSource* source = dataSource();
  vtkImageData* img = vtkImageData::SafeDownCast(
    vtkAlgorithm::SafeDownCast(source->producer()->GetClientSideObject())
      ->GetOutputDataObject(0));
  vtkIdType p1 = img->FindPoint(point1);
  vtkIdType p2 = img->FindPoint(point2);
  double v1 = img->GetPointData()->GetScalars()->GetTuple1(p1);
  double v2 = img->GetPointData()->GetScalars()->GetTuple1(p2);
  emit newEndpointData(v1, v2);
  renderNeeded();
}
}
