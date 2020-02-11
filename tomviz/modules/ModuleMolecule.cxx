/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleMolecule.h"

#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "MoleculeSource.h"
#include "OperatorResult.h"
#include "Utilities.h"

#include <vtkActor.h>
#include <vtkMolecule.h>
#include <vtkMoleculeMapper.h>
#include <vtkNew.h>
#include <vtkRenderer.h>

#include <vtkPVRenderView.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>

namespace tomviz {

ModuleMolecule::ModuleMolecule(QObject* parentObject) : Module(parentObject)
{
}

ModuleMolecule::~ModuleMolecule()
{
  finalize();
}

QIcon ModuleMolecule::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqGroup24.png");
}

bool ModuleMolecule::initialize(OperatorResult* result, vtkSMViewProxy* view)
{
  if (!Module::initialize(result, view)) {
    return false;
  }

  m_molecule = vtkMolecule::SafeDownCast(result->dataObject());
  if (m_molecule == nullptr) {
    return false;
  }

  addMoleculeToView(view);

  return true;
}

bool ModuleMolecule::initialize(MoleculeSource* moleculeSource,
                                vtkSMViewProxy* view)
{
  if (!Module::initialize(moleculeSource, view)) {
    return false;
  }

  m_molecule = vtkMolecule::SafeDownCast(moleculeSource->molecule());
  if (m_molecule == nullptr) {
    return false;
  }

  addMoleculeToView(view);

  return true;
}

void ModuleMolecule::addMoleculeToView(vtkSMViewProxy* view)
{
  m_moleculeMapper->SetInputData(m_molecule);
  m_moleculeActor->SetMapper(m_moleculeMapper);

  m_view = vtkPVRenderView::SafeDownCast(view->GetClientSideView());
  m_view->GetRenderer()->AddActor(m_moleculeActor);
  m_view->Update();
}

bool ModuleMolecule::finalize()
{
  if (m_view) {
    m_view->GetRenderer()->RemoveActor(m_moleculeActor);
  }
  return true;
}

bool ModuleMolecule::setVisibility(bool val)
{
  m_moleculeActor->SetVisibility(val);
  Module::setVisibility(val);
  return true;
}

bool ModuleMolecule::visibility() const
{
  return m_moleculeActor->GetVisibility();
}

void ModuleMolecule::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QFormLayout* layout = new QFormLayout;

  auto ballSlider = new DoubleSliderWidget(true);
  ballSlider->setLineEditWidth(50);
  ballSlider->setMaximum(4.0);
  ballSlider->setValue(m_moleculeMapper->GetAtomicRadiusScaleFactor());
  layout->addRow("Ball Radius", ballSlider);

  auto stickSlider = new DoubleSliderWidget(true);
  stickSlider->setLineEditWidth(50);
  stickSlider->setMaximum(2.0);
  stickSlider->setValue(m_moleculeMapper->GetBondRadius());
  layout->addRow("Stick Radius", stickSlider);

  panel->setLayout(layout);

  connect(ballSlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleMolecule::ballRadiusChanged);

  connect(stickSlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleMolecule::bondRadiusChanged);
}

void ModuleMolecule::ballRadiusChanged(double val)
{
  m_moleculeMapper->SetAtomicRadiusScaleFactor(val);
  if (m_view) {
    m_view->GetRenderer()->Render();
  }
}

void ModuleMolecule::bondRadiusChanged(double val)
{
  m_moleculeMapper->SetBondRadius(val);
  if (m_view) {
    m_view->GetRenderer()->Render();
  }
}

QJsonObject ModuleMolecule::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["ballRadius"] = m_moleculeMapper->GetAtomicRadiusScaleFactor();
  props["stickRadius"] = m_moleculeMapper->GetBondRadius();

  json["properties"] = props;
  return json;
}

bool ModuleMolecule::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    ballRadiusChanged(props["ballRadius"].toDouble());
    bondRadiusChanged(props["stickRadius"].toDouble());
    return true;
  }
  return false;
}

void ModuleMolecule::dataSourceMoved(double, double, double)
{
}

vtkDataObject* ModuleMolecule::dataToExport()
{
  return m_molecule;
}

} // namespace tomviz
