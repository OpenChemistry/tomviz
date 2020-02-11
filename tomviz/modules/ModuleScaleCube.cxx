/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleScaleCube.h"
#include "ModuleScaleCubeWidget.h"

#include "DataSource.h"
#include "Utilities.h"

#include <vtkHandleWidget.h>
#include <vtkMeasurementCubeHandleRepresentation3D.h>

#include <pqCoreUtilities.h>
#include <pqProxiesWidget.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCommand.h>
#include <vtkPVDataInformation.h>
#include <vtkPVRenderView.h>
#include <vtkProperty.h>
#include <vtkSMPVRepresentationProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTextProperty.h>

#include <QCheckBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QJsonArray>
#include <QLineEdit>
#include <QVBoxLayout>

namespace tomviz {

ModuleScaleCube::ModuleScaleCube(QObject* parentObject) : Module(parentObject)
{
  // Connect to m_cubeRep's "modified" signal, and emit it as our own
  // "onPositionChanged" signal
  m_observedPositionId =
    pqCoreUtilities::connect(m_cubeRep, vtkCommand::ModifiedEvent, this,
                             SIGNAL(onPositionChanged()));

  // Connect to our "onPositionChanged" signal and emit it with arguments
  connect(this,
          (void (ModuleScaleCube::*)())(&ModuleScaleCube::onPositionChanged),
          this, [&]() {
            double p[3];
            m_cubeRep->GetWorldPosition(p);
            onPositionChanged(p[0], p[1], p[2]);
          });

  connect(this, SIGNAL(onPositionChanged(double, double, double)),
          SLOT(updateOffset(double, double, double)));

  // Connect to m_cubeRep's "modified" signal, and emit it as our own
  // "onSideLengthChanged" signal
  m_observedSideLengthId =
    pqCoreUtilities::connect(m_cubeRep, vtkCommand::ModifiedEvent, this,
                             SIGNAL(onSideLengthChanged()));

  // Connect to our "onSideLengthChanged" signal and emit it with arguments
  connect(this,
          (void (ModuleScaleCube::*)())(&ModuleScaleCube::onSideLengthChanged),
          this, [&]() { onSideLengthChanged(m_cubeRep->GetSideLength()); });
}

ModuleScaleCube::~ModuleScaleCube()
{
  m_cubeRep->RemoveObserver(m_observedPositionId);
  m_cubeRep->RemoveObserver(m_observedSideLengthId);

  finalize();
}

QIcon ModuleScaleCube::icon() const
{
  return QIcon(":/icons/pqMeasurementCube.png");
}

bool ModuleScaleCube::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  connect(data, SIGNAL(dataPropertiesChanged()), this,
          SLOT(dataPropertiesChanged()));

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_handleWidget->SetInteractor(m_view->GetInteractor());

  double bounds[6];
  dataSource()->proxy()->GetDataInformation()->GetBounds(bounds);
  double length = std::max(floor((bounds[1] - bounds[0]) * .1), 1.);
  m_cubeRep->SetSideLength(length);
  m_cubeRep->SetAdaptiveScaling(0);
  m_cubeRep->SetLengthUnit(data->getUnits().toStdString().c_str());

  m_offset[0] = 0.5 * length;
  m_offset[1] = 0.5 * length;
  m_offset[2] = 0.5 * length;

  const double* displayPosition = dataSource()->displayPosition();
  dataSourceMoved(displayPosition[0], displayPosition[1], displayPosition[2]);

  m_handleWidget->SetRepresentation(m_cubeRep);
  m_handleWidget->EnabledOn();

  return true;
}

bool ModuleScaleCube::finalize()
{
  return true;
}

bool ModuleScaleCube::visibility() const
{
  return m_cubeRep->GetHandleVisibility() == 1;
}

bool ModuleScaleCube::setVisibility(bool choice)
{
  m_cubeRep->SetHandleVisibility(choice ? 1 : 0);
  if (!choice || m_annotationVisibility) {
    m_cubeRep->SetLabelVisibility(choice ? 1 : 0);
  }

  Module::setVisibility(choice);

  return true;
}
QJsonObject ModuleScaleCube::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();
  props["adaptiveScaling"] = m_cubeRep->GetAdaptiveScaling() == 1;
  props["sideLength"] = m_cubeRep->GetSideLength();
  double p[3];
  m_cubeRep->GetWorldPosition(p);
  QJsonArray position = { p[0], p[1], p[2] };
  props["position"] = position;
  props["annotation"] = m_cubeRep->GetLabelVisibility() == 1;
  double c[3];
  m_cubeRep->GetProperty()->GetDiffuseColor(c);
  QJsonArray color = { c[0], c[1], c[2] };
  props["color"] = color;

  m_cubeRep->GetLabelText()->GetTextProperty()->GetColor(c);
  color = { c[0], c[1], c[2] };
  props["textColor"] = color;

  json["properties"] = props;

  return json;
}

bool ModuleScaleCube::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    m_cubeRep->SetAdaptiveScaling(props["adaptiveScaling"].toBool() ? 1 : 0);
    m_cubeRep->SetSideLength(props["sideLength"].toDouble());
    auto p = props["position"].toArray();
    double pos[3] = { p[0].toDouble(), p[1].toDouble(), p[2].toDouble() };
    m_cubeRep->SetWorldPosition(pos);
    m_cubeRep->SetLabelVisibility(props["annotation"].toBool() ? 1 : 0);
    auto c = props["color"].toArray();
    double color[3] = { c[0].toDouble(), c[1].toDouble(), c[2].toDouble() };
    m_cubeRep->GetProperty()->SetDiffuseColor(color);

    if (props["textColor"].isArray()) {
      // This property was added later on...
      c = props["textColor"].toArray();
      double textColor[3] = { c[0].toDouble(), c[1].toDouble(),
                              c[2].toDouble() };
      m_cubeRep->GetLabelText()->GetTextProperty()->SetColor(textColor);
      if (m_controllers) {
        QColor qTextColor(static_cast<int>(textColor[0] * 255.0 + 0.5),
                          static_cast<int>(textColor[1] * 255.0 + 0.5),
                          static_cast<int>(textColor[2] * 255.0 + 0.5));
        m_controllers->setTextColor(qTextColor);
      }
    }

    if (m_controllers) {
      QColor qColor(static_cast<int>(color[0] * 255.0 + 0.5),
                    static_cast<int>(color[1] * 255.0 + 0.5),
                    static_cast<int>(color[2] * 255.0 + 0.5));
      m_controllers->setBoxColor(qColor);
      m_controllers->setAdaptiveScaling(props["adaptiveScaling"].toBool());
    }
    return true;
  }
  return false;
}

void ModuleScaleCube::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  m_controllers = new ModuleScaleCubeWidget;
  layout->addWidget(m_controllers);

  // Set initial parameters
  m_controllers->setAdaptiveScaling(
    static_cast<bool>(m_cubeRep->GetAdaptiveScaling()));
  m_controllers->setSideLength(m_cubeRep->GetSideLength());
  m_controllers->setAnnotation(
    static_cast<bool>(m_cubeRep->GetLabelVisibility()));
  m_controllers->setLengthUnit(QString(m_cubeRep->GetLengthUnit()));
  double worldPosition[3];
  m_cubeRep->GetWorldPosition(worldPosition);
  m_controllers->setPosition(worldPosition[0], worldPosition[1],
                             worldPosition[2]);
  m_controllers->setPositionUnit(QString(m_cubeRep->GetLengthUnit()));
  double color[3];
  m_cubeRep->GetProperty()->GetDiffuseColor(color);
  m_controllers->setBoxColor(QColor(static_cast<int>(color[0] * 255.0 + 0.5),
                                    static_cast<int>(color[1] * 255.0 + 0.5),
                                    static_cast<int>(color[2] * 255.0 + 0.5)));

  m_cubeRep->GetLabelText()->GetTextProperty()->GetColor(color);
  m_controllers->setTextColor(QColor(static_cast<int>(color[0] * 255.0 + 0.5),
                                     static_cast<int>(color[1] * 255.0 + 0.5),
                                     static_cast<int>(color[2] * 255.0 + 0.5)));

  // Connect the widget's signals to this class' slots
  connect(m_controllers, SIGNAL(adaptiveScalingToggled(const bool)), this,
          SLOT(setAdaptiveScaling(const bool)));
  connect(m_controllers, SIGNAL(sideLengthChanged(const double)), this,
          SLOT(setSideLength(const double)));
  connect(m_controllers, SIGNAL(annotationToggled(const bool)), this,
          SLOT(setAnnotation(const bool)));
  connect(m_controllers, &ModuleScaleCubeWidget::boxColorChanged, this,
          &ModuleScaleCube::onBoxColorChanged);
  connect(m_controllers, &ModuleScaleCubeWidget::textColorChanged, this,
          &ModuleScaleCube::onTextColorChanged);

  // Connect this class' signals to the widget's slots
  connect(this, SIGNAL(onLengthUnitChanged(const QString)), m_controllers,
          SLOT(setLengthUnit(const QString)));
  connect(this, SIGNAL(onPositionUnitChanged(const QString)), m_controllers,
          SLOT(setPositionUnit(const QString)));
  connect(this, SIGNAL(onSideLengthChanged(const double)), m_controllers,
          SLOT(setSideLength(const double)));
  connect(
    this, SIGNAL(onPositionChanged(const double, const double, const double)),
    m_controllers, SLOT(setPosition(const double, const double, const double)));
}

void ModuleScaleCube::setAdaptiveScaling(const bool val)
{
  m_cubeRep->SetAdaptiveScaling(val ? 1 : 0);
}

void ModuleScaleCube::setSideLength(const double length)
{
  m_cubeRep->SetSideLength(length);
  emit renderNeeded();
}

void ModuleScaleCube::setAnnotation(const bool val)
{
  m_cubeRep->SetLabelVisibility(val ? 1 : 0);
  m_annotationVisibility = val;
  emit renderNeeded();
}

void ModuleScaleCube::setLengthUnit()
{
  QString s = qobject_cast<DataSource*>(sender())->getUnits();
  m_cubeRep->SetLengthUnit(s.toStdString().c_str());
  emit onLengthUnitChanged(s);
}

void ModuleScaleCube::setPositionUnit()
{
  QString s = qobject_cast<DataSource*>(sender())->getUnits();
  emit onLengthUnitChanged(s);
}

void ModuleScaleCube::dataPropertiesChanged()
{
  DataSource* data = qobject_cast<DataSource*>(sender());
  if (!data) {
    return;
  }
  m_cubeRep->SetLengthUnit(data->getUnits().toStdString().c_str());

  emit onLengthUnitChanged(data->getUnits());
  emit onPositionUnitChanged(data->getUnits());
}

void ModuleScaleCube::dataSourceMoved(double newX, double newY, double newZ)
{
  double position[3];
  position[0] = newX + m_offset[0];
  position[1] = newY + m_offset[1];
  position[2] = newZ + m_offset[2];

  m_cubeRep->PlaceWidget(position);
  m_cubeRep->SetWorldPosition(position);
}

void ModuleScaleCube::onBoxColorChanged(const QColor& color)
{
  m_cubeRep->GetProperty()->SetDiffuseColor(
    color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0);
  emit renderNeeded();
}

void ModuleScaleCube::onTextColorChanged(const QColor& color)
{
  m_cubeRep->GetLabelText()->GetTextProperty()->SetColor(
    color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0);
  emit renderNeeded();
}

void ModuleScaleCube::updateOffset(double x, double y, double z)
{
  const double* displayPosition = dataSource()->displayPosition();
  m_offset[0] = x - displayPosition[0];
  m_offset[1] = y - displayPosition[1];
  m_offset[2] = z - displayPosition[2];
}

} // end of namespace tomviz
