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
#include <vtkTrivialProducer.h>

#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace tomviz {

ModuleRuler::ModuleRuler(QObject* p) : Module(p)
{
}

ModuleRuler::~ModuleRuler()
{
  finalize();
}

QIcon ModuleRuler::icon() const
{
  return QIcon(":/icons/pqRuler.png");
}

bool ModuleRuler::initialize(DataSource* data, vtkSMViewProxy* view)
{
  if (!Module::initialize(data, view)) {
    return false;
  }
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  auto pxm = data->proxy()->GetSessionProxyManager();
  auto alg = vtkAlgorithm::SafeDownCast(data->producer());
  double bounds[6];
  vtkDataSet::SafeDownCast(alg->GetOutputDataObject(0))->GetBounds(bounds);
  double boundsMin[3] = { bounds[0], bounds[2], bounds[4] };
  double boundsMax[3] = { bounds[1], bounds[3], bounds[5] };

  m_rulerSource.TakeReference(
    vtkSMSourceProxy::SafeDownCast(pxm->NewProxy("sources", "Ruler")));
  vtkSMPropertyHelper(m_rulerSource, "Point1").Set(boundsMin, 3);
  vtkSMPropertyHelper(m_rulerSource, "Point2").Set(boundsMax, 3);
  m_rulerSource->UpdateVTKObjects();
  controller->RegisterPipelineProxy(m_rulerSource);

  m_representation = controller->Show(m_rulerSource, 0, view);

  m_representation->UpdateVTKObjects();

  updateUnits();

  connect(data, &DataSource::dataChanged, this, &ModuleRuler::updateUnits);

  return m_representation && m_rulerSource;
}

bool ModuleRuler::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_representation);
  controller->UnRegisterProxy(m_rulerSource);
  m_representation = nullptr;
  m_rulerSource = nullptr;
  return true;
}

void ModuleRuler::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }
  QVBoxLayout* layout = new QVBoxLayout;

  m_widget = new pqLinePropertyWidget(
    m_rulerSource, m_rulerSource->GetPropertyGroup(0), panel);
  layout->addWidget(m_widget);
  m_widget->setView(
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView()));
  m_widget->select();
  m_widget->setWidgetVisible(m_showLine);
  layout->addStretch();
  connect(m_widget.data(), &pqPropertyWidget::changeFinished, m_widget.data(),
          &pqPropertyWidget::apply);
  connect(m_widget.data(), &pqPropertyWidget::changeFinished, this,
          &ModuleRuler::endPointsUpdated);
  connect(m_widget, SIGNAL(widgetVisibilityUpdated(bool)), this,
          SLOT(updateShowLine(bool)));

  m_widget->setWidgetVisible(m_showLine);

  QLabel* label0 = new QLabel("Point 0 data value: ");
  QLabel* label1 = new QLabel("Point 1 data value: ");
  connect(this, &ModuleRuler::newEndpointData, label0,
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
  disconnect(m_widget, SIGNAL(widgetVisibilityUpdated(bool)), this,
             SLOT(updateShowLine(bool)));
}

bool ModuleRuler::setVisibility(bool val)
{
  vtkSMPropertyHelper(m_representation, "Visibility").Set(val ? 1 : 0);
  m_representation->UpdateVTKObjects();
  if (m_widget) {
    // calling setWidgetVisible triggers the signal that updates the value of
    // m_showLine.  But in this case the user is toggling the whole module so
    // we don't want m_showLine to update and we cache it locally and restore
    // it after calling setWidgetVisible.
    bool oldValue = m_showLine;
    m_widget->setWidgetVisible(val && m_showLine);
    m_showLine = oldValue;
  }
  return true;
}

bool ModuleRuler::visibility() const
{
  if (m_representation) {
    return vtkSMPropertyHelper(m_representation, "Visibility").GetAsInt() != 0;
  } else {
    return false;
  }
}

QJsonObject ModuleRuler::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["showLine"] = m_showLine;
  double p1[3];
  double p2[3];
  vtkSMPropertyHelper(m_rulerSource, "Point1").Get(p1, 3);
  vtkSMPropertyHelper(m_rulerSource, "Point2").Get(p2, 3);
  QJsonArray point1 = { p1[0], p1[1], p1[2] };
  QJsonArray point2 = { p2[0], p2[1], p2[2] };
  props["point1"] = point1;
  props["point2"] = point2;

  json["properties"] = props;
  return json;
}

bool ModuleRuler::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    m_showLine = props["showLine"].toBool();
    auto point1 = props["point1"].toArray();
    auto point2 = props["point2"].toArray();
    double p1[3] = { point1[0].toDouble(), point1[1].toDouble(),
                     point1[2].toDouble() };
    double p2[3] = { point2[0].toDouble(), point2[1].toDouble(),
                     point2[2].toDouble() };
    vtkSMPropertyHelper(m_rulerSource, "Point1").Set(p1, 3);
    vtkSMPropertyHelper(m_rulerSource, "Point2").Set(p2, 3);
    m_rulerSource->UpdateVTKObjects();
    return true;
  }
  return false;
}

bool ModuleRuler::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return proxy == m_rulerSource.GetPointer() ||
         proxy == m_representation.GetPointer();
}

std::string ModuleRuler::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_rulerSource.GetPointer()) {
    return "Ruler";
  } else if (proxy == m_representation.GetPointer()) {
    return "Representation";
  } else {
    qWarning("Unknown proxy passed to module ruler in save animation");
    return "";
  }
}

vtkSMProxy* ModuleRuler::getProxyForString(const std::string& str)
{
  if (str == "Ruler") {
    return m_rulerSource;
  } else if (str == "Representation") {
    return m_representation;
  } else {
    return nullptr;
  }
}

void ModuleRuler::updateUnits()
{
  DataSource* source = dataSource();
  QString units = source->getUnits();
  vtkRulerSourceRepresentation* rep =
    vtkRulerSourceRepresentation::SafeDownCast(
      m_representation->GetClientSideObject());
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
  vtkSMPropertyHelper(m_rulerSource, "Point1").Get(point1, 3);
  vtkSMPropertyHelper(m_rulerSource, "Point2").Get(point2, 3);
  DataSource* source = dataSource();
  vtkImageData* img = vtkImageData::SafeDownCast(
    vtkAlgorithm::SafeDownCast(source->proxy()->GetClientSideObject())
      ->GetOutputDataObject(0));
  vtkIdType p1 = img->FindPoint(point1);
  vtkIdType p2 = img->FindPoint(point2);
  double v1 = img->GetPointData()->GetScalars()->GetTuple1(p1);
  double v2 = img->GetPointData()->GetScalars()->GetTuple1(p2);
  emit newEndpointData(v1, v2);
  renderNeeded();
}
}
