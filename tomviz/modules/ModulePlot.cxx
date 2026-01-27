/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModulePlot.h"

#include "OperatorResult.h"
#include "Utilities.h"

#include <vtkCallbackCommand.h>
#include <vtkChartXY.h>
#include <vtkContextView.h>
#include <vtkPlot.h>
#include <vtkPlotLine.h>
#include <vtkPVContextView.h>
#include <vtkSMViewProxy.h>
#include <vtkTable.h>
#include <vtkTrivialProducer.h>

#include <QFormLayout>
#include <QJsonObject>
#include <QWidget>

namespace tomviz {

ModulePlot::ModulePlot(QObject* parentObject)
  : Module(parentObject)
  , m_visible(true)
  , m_view(nullptr)
  , m_table(nullptr)
  , m_chart(nullptr)
  , m_producer(nullptr)
{
  m_result_modified_cb->SetCallback(&ModulePlot::onResultModified);
  m_result_modified_cb->SetClientData(this);
}

ModulePlot::~ModulePlot()
{
  finalize();
}

QIcon ModulePlot::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqLineChart16.png");
}

bool ModulePlot::initialize(DataSource* data, vtkSMViewProxy* view)
{
  Q_UNUSED(data);
  Q_UNUSED(view);

  return false;
}

bool ModulePlot::initialize(MoleculeSource* data, vtkSMViewProxy* view)
{
  Q_UNUSED(data);
  Q_UNUSED(view);

  return false;
}

bool ModulePlot::initialize(OperatorResult* result, vtkSMViewProxy* view)
{
  Module::initialize(result, view);

  m_table = vtkTable::SafeDownCast(result->dataObject());
  m_producer = vtkTrivialProducer::SafeDownCast(result->producerProxy()->GetClientSideObject());
  m_view = vtkPVContextView::SafeDownCast(view->GetClientSideView());
  m_chart = nullptr;

  if (m_table == nullptr || m_producer == nullptr || m_view == nullptr) {
    return false;
  }

  auto context_view = m_view->GetContextView();

  m_chart = vtkChartXY::SafeDownCast(context_view->GetScene()->GetItem(0));

  if (m_chart == nullptr) {
    return false;
  }

  // Detect when the result dataobject changes, i.e. when the pipeline re-runs
  m_producer->AddObserver(vtkCommand::ModifiedEvent, m_result_modified_cb);

  addAllPlots();

  return true;
}

bool ModulePlot::finalize()
{
  if (m_producer) {
    m_producer->RemoveObserver(m_result_modified_cb);
  }

  removeAllPlots();

  m_plots.clear();

  return true;
}

void ModulePlot::addAllPlots()
{
  removeAllPlots();

  if (m_table == nullptr || m_chart == nullptr) {
    return;
  }

  vtkIdType num_cols = m_table->GetNumberOfColumns();

  for (vtkIdType col = 1; col < num_cols; col++) {
    auto line = vtkSmartPointer<vtkPlotLine>::New();
    u_int8_t color[3] = {0, 0, 0};
    color[(col - 1) % 3] = 255;
    line->SetInputData(m_table, 0, col);
    line->SetColor(color[0], color[1], color[2], 255);
    line->SetWidth(3.0);
    m_chart->AddPlot(line);
    m_plots.append(line);
  }
}

void ModulePlot::removeAllPlots()
{
  if (m_chart == nullptr) {
    return;
  }

  for (auto iter = m_plots.begin(); iter != m_plots.end(); iter++) {
    m_chart->RemovePlotInstance(*iter);
  }

  m_plots.clear();
}

bool ModulePlot::setVisibility(bool val)
{
  m_visible = val;

  if (val) {
    addAllPlots();
  } else {
    removeAllPlots();
  }

  Module::setVisibility(val);

  return true;
}

bool ModulePlot::visibility() const
{
  return m_visible;
}

void ModulePlot::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QFormLayout* layout = new QFormLayout;

  panel->setLayout(layout);
}

QJsonObject ModulePlot::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  json["properties"] = props;
  return json;
}

bool ModulePlot::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    return true;
  }
  return false;
}

void ModulePlot::dataSourceMoved(double, double, double)
{
}

void ModulePlot::dataSourceRotated(double, double, double)
{
}

vtkDataObject* ModulePlot::dataToExport()
{
  return nullptr;
}

void ModulePlot::onResultModified(vtkObject* caller, long unsigned int eventId, void* clientData, void*callData)
{
  Q_UNUSED(caller);
  Q_UNUSED(eventId);
  Q_UNUSED(callData);

  auto self = reinterpret_cast<ModulePlot*>(clientData);
  auto result = self->operatorResult();
  self->m_table = vtkTable::SafeDownCast(result->dataObject());

  self->removeAllPlots();
  self->m_plots.clear();

  if (self->visibility()) {
    self->addAllPlots();
  }
}

} // namespace tomviz
