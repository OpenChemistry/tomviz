/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RulerSink.h"

#include "data/VolumeData.h"

#include <pqApplicationCore.h>
#include <pqLinePropertyWidget.h>
#include <pqPointPickingHelper.h>
#include <pqServerManagerModel.h>
#include <pqView.h>

#include <QLabel>

#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkLineSource.h>
#include <vtkPVRenderView.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>

#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <cstdio>

namespace tomviz {
namespace pipeline {

RulerSink::RulerSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Ruler");

  // Set up the persistent line visual.
  m_mapper->SetInputConnection(m_lineSource->GetOutputPort());
  m_lineActor->SetMapper(m_mapper);
  m_lineActor->SetVisibility(0);
  m_lineActor->GetProperty()->SetLineWidth(2.0);
  m_lineActor->GetProperty()->SetColor(0.0, 1.0, 0.0);

  // Set up the distance label.
  m_textActor->SetVisibility(0);
  m_textActor->GetTextProperty()->SetFontSize(18);
  m_textActor->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
  m_textActor->GetTextProperty()->SetJustificationToCentered();
}

RulerSink::~RulerSink()
{
  finalize();
}

QIcon RulerSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqRuler.svg"));
}

void RulerSink::setVisibility(bool visible)
{
  m_lineActor->SetVisibility(visible ? 1 : 0);
  m_textActor->SetVisibility(visible ? 1 : 0);

  if (m_widget) {
    // Calling setWidgetVisible triggers widgetVisibilityUpdated which
    // would overwrite m_showLine.  But here the user is toggling the
    // whole module, so cache and restore m_showLine.
    bool oldValue = m_showLine;
    m_widget->setWidgetVisible(visible && m_showLine);
    m_showLine = oldValue;
  }

  LegacyModuleSink::setVisibility(visible);
}

bool RulerSink::initialize(vtkSMViewProxy* vtkView)
{
  if (!LegacyModuleSink::initialize(vtkView)) {
    return false;
  }

  renderView()->AddPropToRenderer(m_lineActor);
  renderView()->AddPropToRenderer(m_textActor);
  return true;
}

bool RulerSink::finalize()
{
  if (renderView()) {
    renderView()->RemovePropFromRenderer(m_lineActor);
    renderView()->RemovePropFromRenderer(m_textActor);
  }

  m_rulerSource = nullptr;

  return LegacyModuleSink::finalize();
}

void RulerSink::clearVisualization()
{
  m_lineActor->SetVisibility(0);
  m_textActor->SetVisibility(0);
}

bool RulerSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  m_pendingImage = volume->imageData();

  // SM proxy work must happen on the GUI thread; consume() runs there
  // now (consumeOnGuiThread()).
  setupOrUpdatePipeline();

  return true;
}

void RulerSink::setupOrUpdatePipeline()
{
  if (!m_pendingImage || !view()) {
    return;
  }

  if (!m_rulerSource) {
    // --- First time: create SM source proxy for pqLinePropertyWidget ---
    auto* pxm = view()->GetSessionProxyManager();

    double bounds[6];
    m_pendingImage->GetBounds(bounds);
    double p1[3] = { bounds[0], bounds[2], bounds[4] };
    double p2[3] = { bounds[1], bounds[3], bounds[5] };

    if (m_hasPendingPoints) {
      std::copy(m_pendingPoint1, m_pendingPoint1 + 3, p1);
      std::copy(m_pendingPoint2, m_pendingPoint2 + 3, p2);
      m_hasPendingPoints = false;
    }

    m_rulerSource.TakeReference(
      vtkSMSourceProxy::SafeDownCast(pxm->NewProxy("sources", "Ruler")));
    vtkSMPropertyHelper(m_rulerSource, "Point1").Set(p1, 3);
    vtkSMPropertyHelper(m_rulerSource, "Point2").Set(p2, 3);
    m_rulerSource->UpdateVTKObjects();

    // Do NOT register as a pipeline proxy or call controller->Show().
    // RegisterPipelineProxy with the rendering controller can auto-create
    // a vtkRulerSourceRepresentation that interferes with the volume
    // mapper's shader during selection renders, and the representation
    // itself crashes in ProcessViewRequest().  The proxy is only needed
    // for pqLinePropertyWidget binding, which works without registration.

    // Sync the line and label with the initial points.
    m_lineSource->SetPoint1(p1);
    m_lineSource->SetPoint2(p2);
  }

  updateUnits();
  updateRulerVisual();
  m_lineActor->SetVisibility(visibility() ? 1 : 0);
  m_textActor->SetVisibility(visibility() ? 1 : 0);
  m_pendingImage = nullptr;
  emit renderNeeded();
}

void RulerSink::updateRulerVisual()
{
  if (!m_rulerSource) {
    return;
  }

  double p1[3];
  double p2[3];
  vtkSMPropertyHelper(m_rulerSource, "Point1").Get(p1, 3);
  vtkSMPropertyHelper(m_rulerSource, "Point2").Get(p2, 3);

  m_lineSource->SetPoint1(p1);
  m_lineSource->SetPoint2(p2);

  // Distance label at the midpoint.
  double dx = p2[0] - p1[0];
  double dy = p2[1] - p1[1];
  double dz = p2[2] - p1[2];
  double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

  char buf[128];
  std::snprintf(buf, sizeof(buf), "%-#6.3g", dist);
  QString label = m_units.isEmpty() ? QString::fromLatin1(buf)
                                    : QString("%1 %2").arg(buf, m_units);
  m_textActor->SetInput(label.toLatin1().data());

  double mid[3] = { (p1[0] + p2[0]) / 2.0, (p1[1] + p2[1]) / 2.0,
                     (p1[2] + p2[2]) / 2.0 };
  m_textActor->SetPosition(mid);
}

void RulerSink::updateUnits()
{
  auto vol = volumeData();
  m_units = vol ? vol->units() : QString();
}

void RulerSink::onMetadataChanged()
{
  updateUnits();
  updateRulerVisual();
  emit renderNeeded();
}

void RulerSink::endPointsUpdated()
{
  // Sync the persistent visual with the (just-updated) proxy properties.
  updateRulerVisual();

  if (!m_rulerSource) {
    return;
  }

  double point1[3];
  double point2[3];
  vtkSMPropertyHelper(m_rulerSource, "Point1").Get(point1, 3);
  vtkSMPropertyHelper(m_rulerSource, "Point2").Get(point2, 3);

  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }
  vtkImageData* img = vol->imageData();
  if (!img) {
    return;
  }
  vtkDataArray* scalars = img->GetPointData()->GetScalars();
  if (!scalars) {
    return;
  }
  vtkIdType p1 = img->FindPoint(point1);
  vtkIdType p2 = img->FindPoint(point2);
  double v1 = scalars->GetTuple1(p1);
  double v2 = scalars->GetTuple1(p2);
  emit newEndpointData(v1, v2);
  emit renderNeeded();
}

QWidget* RulerSink::createSinkPropertiesWidget(QWidget* parent)
{
  if (!m_rulerSource) {
    return nullptr;
  }

  auto* widget = new QWidget(parent);
  auto* layout = new QVBoxLayout;

  m_widget = new pqLinePropertyWidget(
    m_rulerSource, m_rulerSource->GetPropertyGroup(0), widget);
  m_widget->setFrameShape(QFrame::NoFrame);
  m_widget->setLineColor(QColor(0, 255, 0));
  layout->addWidget(m_widget);

  auto* smModel = pqApplicationCore::instance()->getServerManagerModel();
  auto* pqview = smModel->findItem<pqView*>(view());
  if (pqview) {
    m_widget->setView(pqview);
  }
  m_widget->select();
  m_widget->setWidgetVisible(m_showLine);

  // Disable point picking helpers. The 'P' shortcut registered by
  // pqLinePropertyWidget triggers selection renders that expose a VTK
  // volume shader compilation bug on certain GPU drivers.
  for (auto* helper : m_widget->findChildren<pqPointPickingHelper*>()) {
    helper->setView(nullptr);
  }
  // The panel's note lists those picking shortcuts; keep only the
  // constraint keys, which the line widget handles itself
  if (auto* note = m_widget->findChild<QLabel*>("pickLabel")) {
    note->setText("Hold 'X', 'Y' or 'Z' while dragging an end to move it "
                  "only along that axis, or 'L' to move it along the line.");
  }

  layout->addStretch();

  connect(m_widget.data(), &pqPropertyWidget::changeFinished,
          m_widget.data(), &pqPropertyWidget::apply);
  connect(m_widget.data(), &pqPropertyWidget::changeFinished, this,
          &RulerSink::endPointsUpdated);

  auto visConn = connect(
    m_widget,
    &pqInteractivePropertyWidgetAbstract::widgetVisibilityUpdated, this,
    [this](bool show) { m_showLine = show; });

  // Disconnect before children are destroyed to prevent a false
  // m_showLine update during widget teardown (same purpose as the old
  // ModuleRuler::prepareToRemoveFromPanel).  QObject::destroyed() is
  // emitted at the top of ~QObject, before children are deleted.
  connect(widget, &QObject::destroyed, this,
          [visConn]() { QObject::disconnect(visConn); });

  auto* label0 = new QLabel("Point 0 data value: ");
  auto* label1 = new QLabel("Point 1 data value: ");
  connect(this, &RulerSink::newEndpointData, label0,
          [label0, label1](double val0, double val1) {
            label0->setText(QString("Point 0 data value: %1").arg(val0));
            label1->setText(QString("Point 1 data value: %1").arg(val1));
          });
  layout->addWidget(label0);
  layout->addWidget(label1);

  widget->setLayout(layout);
  return widget;
}

QJsonObject RulerSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["showLine"] = m_showLine;
  if (m_rulerSource) {
    double p1[3];
    double p2[3];
    vtkSMPropertyHelper(m_rulerSource, "Point1").Get(p1, 3);
    vtkSMPropertyHelper(m_rulerSource, "Point2").Get(p2, 3);
    QJsonArray point1 = { p1[0], p1[1], p1[2] };
    QJsonArray point2 = { p2[0], p2[1], p2[2] };
    json["point1"] = point1;
    json["point2"] = point2;
  }
  return json;
}

bool RulerSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("showLine")) {
    m_showLine = json["showLine"].toBool();
  }
  if (json.contains("point1")) {
    auto point1 = json["point1"].toArray();
    auto point2 = json["point2"].toArray();
    m_pendingPoint1[0] = point1[0].toDouble();
    m_pendingPoint1[1] = point1[1].toDouble();
    m_pendingPoint1[2] = point1[2].toDouble();
    m_pendingPoint2[0] = point2[0].toDouble();
    m_pendingPoint2[1] = point2[1].toDouble();
    m_pendingPoint2[2] = point2[2].toDouble();
    m_hasPendingPoints = true;
    // Apply immediately if the SM proxy already exists.
    if (m_rulerSource) {
      vtkSMPropertyHelper(m_rulerSource, "Point1").Set(m_pendingPoint1, 3);
      vtkSMPropertyHelper(m_rulerSource, "Point2").Set(m_pendingPoint2, 3);
      m_rulerSource->UpdateVTKObjects();
      m_hasPendingPoints = false;
      updateRulerVisual();
    }
  }
  return true;
}

} // namespace pipeline
} // namespace tomviz
