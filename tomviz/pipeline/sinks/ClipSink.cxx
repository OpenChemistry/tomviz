/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ClipSink.h"

#include "DoubleSliderWidget.h"
#include "InputPort.h"
#include "IntSliderWidget.h"
#include "Link.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "PlaneIndexing.h"
#include "Port.h"
#include "data/VolumeData.h"
#include "vtkNonOrthoImagePlaneWidget.h"

#include <pqColorChooserButton.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <limits>

#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPVRenderView.h>
#include <vtkPlane.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSMViewProxy.h>
#include <vtkTransform.h>
#include <vtkTrivialProducer.h>

#include <algorithm>
#include <cmath>

namespace tomviz {
namespace pipeline {

void planeTravelRange(const double bounds[6], const double normal[3],
                      double& minDistance, double& maxDistance)
{
  minDistance = 0.0;
  maxDistance = 0.0;

  double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] +
                            normal[2] * normal[2]);
  if (length == 0.0) {
    return;
  }

  double n[3] = { normal[0] / length, normal[1] / length,
                  normal[2] / length };
  double center[3] = { (bounds[0] + bounds[1]) / 2.0,
                       (bounds[2] + bounds[3]) / 2.0,
                       (bounds[4] + bounds[5]) / 2.0 };

  // The extreme distances are always reached at corners, so the eight of
  // them bound every point in the box.
  for (int i = 0; i < 8; ++i) {
    double corner[3] = { bounds[i & 1], bounds[2 + ((i >> 1) & 1)],
                         bounds[4 + ((i >> 2) & 1)] };
    double d = (corner[0] - center[0]) * n[0] +
               (corner[1] - center[1]) * n[1] +
               (corner[2] - center[2]) * n[2];
    minDistance = std::min(minDistance, d);
    maxDistance = std::max(maxDistance, d);
  }
}

namespace {

// Set while a linked clip is pushing its state onto its peers, so their
// own change signals do not echo it back. GUI thread only.
bool s_propagatingClipLink = false;

} // namespace

ClipSink::ClipSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Clip");

  connect(inputPort("volume"), &Port::connectionChanged,
          this, &ClipSink::onInputConnectionChanged);

  connect(this, &ClipSink::sliceChanged,
          this, &ClipSink::propagateToLinkedSinks);
}

ClipSink::~ClipSink()
{
  disconnectFromSiblings();
  finalize();
}

QIcon ClipSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqClip.svg"));
}

void ClipSink::setVisibility(bool visible)
{
  if (m_widget) {
    m_widget->SetEnabled(visible ? 1 : 0);
    if (visible) {
      // A hidden plane hides its arrow too, whatever Show Arrow says
      const bool arrow = m_showPlane && m_showArrow;
      m_widget->SetArrowVisibility(arrow ? 1 : 0);
      m_widget->SetInteraction(arrow ? 1 : 0);
    }
  }

  // Apply or remove clipping plane on siblings based on visibility
  for (auto* sink : m_clippedSinks) {
    if (visible) {
      sink->addClippingPlane(m_clippingPlane);
    } else {
      sink->removeClippingPlane(m_clippingPlane);
    }
  }

  LegacyModuleSink::setVisibility(visible);
}

void ClipSink::setupWidget()
{
  m_widget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();
  m_widget->TextureInterpolateOn();
  m_widget->SetResliceInterpolateToLinear();

  // Grayscale LUT (saturation=0, value=1) like old ModuleClip
  vtkNew<vtkColorTransferFunction> lut;
  lut->SetColorSpaceToHSV();
  lut->HSVWrapOff();
  lut->AddHSVPoint(0.0, 0.0, 0.0, 1.0);
  lut->AddHSVPoint(1.0, 0.0, 0.0, 1.0);
  m_widget->SetLookupTable(lut);

  // Set plane color to off-white
  auto* prop = m_widget->GetPlaneProperty();
  prop->SetColor(204.0 / 255, 204.0 / 255, 204.0 / 255);

  m_widget->SetOpacity(m_opacity);

  // When the user drags the widget, sync m_clippingPlane from it
  vtkNew<vtkCallbackCommand> callback;
  callback->SetClientData(this);
  callback->SetCallback([](vtkObject*, unsigned long, void* clientData, void*) {
    static_cast<ClipSink*>(clientData)->onWidgetInteraction();
  });
  m_interactionTag =
    m_widget->AddObserver(vtkCommand::InteractionEvent, callback);

  vtkNew<vtkCallbackCommand> startCallback;
  startCallback->SetClientData(this);
  startCallback->SetCallback(
    [](vtkObject*, unsigned long, void* clientData, void*) {
      static_cast<ClipSink*>(clientData)->onWidgetInteractionStarted();
    });
  m_startInteractionTag =
    m_widget->AddObserver(vtkCommand::StartInteractionEvent, startCallback);
}

bool ClipSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  setupWidget();

  auto* renderWindow = view->GetRenderWindow();
  if (renderWindow) {
    auto* rwi = renderWindow->GetInteractor();
    if (rwi) {
      m_widget->SetInteractor(rwi);

      // Provide a minimal input before On() so the widget can create its
      // texture object. Without input data, On() triggers a
      // "Could not find the vtkTextureObject" error. The real data replaces
      // this in consume() almost immediately.
      vtkNew<vtkImageData> placeholder;
      placeholder->SetDimensions(2, 2, 2);
      placeholder->AllocateScalars(VTK_FLOAT, 1);
      vtkNew<vtkTrivialProducer> placeholderProducer;
      placeholderProducer->SetOutput(placeholder);
      m_widget->SetInputConnection(placeholderProducer->GetOutputPort());

      m_widget->On();
      m_widget->InteractionOn();
      m_widget->SetArrowVisibility(m_showPlane && m_showArrow ? 1 : 0);
    }
  }

  return true;
}

bool ClipSink::finalize()
{
  if (m_widget) {
    if (m_interactionTag) {
      m_widget->RemoveObserver(m_interactionTag);
      m_interactionTag = 0;
    }
    if (m_startInteractionTag) {
      m_widget->RemoveObserver(m_startInteractionTag);
      m_startInteractionTag = 0;
    }
    // Order matters: InteractionOff/Off require a valid interactor,
    // so call them before clearing it.
    if (m_widget->GetInteractor() && m_widget->GetEnabled()) {
      m_widget->InteractionOff();
      m_widget->Off();
    }
    m_widget->SetInteractor(nullptr);
    m_widget = nullptr;
  }
  return LegacyModuleSink::finalize();
}

void ClipSink::clearVisualization()
{
  if (m_widget && m_widget->GetInteractor()) {
    m_widget->Off();
  }
}

bool ClipSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  auto* imageData = volume->imageData();
  imageData->GetDimensions(m_dims);
  imageData->GetBounds(m_bounds);

  if (m_widget) {
    vtkNew<vtkTrivialProducer> producer;
    producer->SetOutput(imageData);
    m_widget->SetInputConnection(producer->GetOutputPort());

    applyDirection();
    syncClippingPlane();

    // SetEnabled() touches the renderer and Qt GL state. consume() runs
    // on the GUI thread (consumeOnGuiThread()), so apply directly.
    m_widget->SetEnabled(visibility() ? 1 : 0);
  } else {
    // Without widget: just update clipping plane from stored direction
    double cx = (m_bounds[0] + m_bounds[1]) / 2.0;
    double cy = (m_bounds[2] + m_bounds[3]) / 2.0;
    double cz = (m_bounds[4] + m_bounds[5]) / 2.0;
    m_clippingPlane->SetOrigin(cx, cy, cz);
    double n[3] = { 0, 0, 1 };
    if (m_direction == YZ) {
      n[0] = 1; n[1] = 0; n[2] = 0;
    } else if (m_direction == XZ) {
      n[0] = 0; n[1] = 1; n[2] = 0;
    }
    if (m_invertPlane) {
      n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2];
    }
    m_clippingPlane->SetNormal(n);
  }

  auto vol = volumeData();
  if (vol && vol->isValid()) {
    m_lastSpacing = vol->spacing();
    m_lastOrigin = vol->origin();
  }

  onMetadataChanged();
  emit clipPlaneUpdated();
  emit renderNeeded();
  return true;
}

void ClipSink::applyDirection()
{
  if (!m_widget) {
    return;
  }

  int axis = directionAxis();
  if (axis >= 0) {
    // Orthogonal direction
    m_widget->SetPlaneOrientation(axis);

    // Apply invert: SetPlaneOrientation sets a default +axis normal,
    // so flip it if inverted (matching old ModuleClip::onDirectionChanged).
    if (m_invertPlane) {
      double normal[3] = { 0, 0, 0 };
      normal[axis] = -1;
      m_widget->SetNormal(normal);
    }

    if (m_slice < 0) {
      m_slice = m_dims[axis] / 2;
    }
    m_slice = qBound(0, m_slice, m_dims[axis] - 1);
    m_widget->SetSliceIndex(m_slice);
  } else {
    // Custom direction — SetPlaneOrientation with a non-0/1/2 value puts
    // the widget into custom mode: it resizes the plane to span the full
    // image while preserving the current normal. This must be called so
    // the arrow interaction works for arbitrary orientations.
    m_widget->SetPlaneOrientation(axis); // axis is -1 for Custom
    m_widget->UpdatePlacement();
  }
}

ClipSink::Direction ClipSink::direction() const
{
  return m_direction;
}

void ClipSink::setDirection(Direction dir)
{
  if (m_direction == dir) {
    return;
  }
  m_direction = dir;
  applyDirection();
  syncClippingPlane();
  emit directionChanged(dir);
  emit clipPlaneUpdated();
  emit renderNeeded();
  propagateToLinkedSinks();
}

int ClipSink::slice() const
{
  return m_slice;
}

void ClipSink::setSlice(int s)
{
  m_slice = s;
  if (m_widget && isOrtho()) {
    int axis = directionAxis();
    m_slice = qBound(0, m_slice, m_dims[axis] - 1);
    m_widget->SetSliceIndex(m_slice);
  }
  syncClippingPlane();
  emit sliceChanged(m_slice);
  emit clipPlaneUpdated();
  emit renderNeeded();
}

double ClipSink::opacity() const
{
  return m_opacity;
}

void ClipSink::setOpacity(double value)
{
  m_opacity = value;
  if (m_widget) {
    m_widget->SetOpacity(value);
  }
  emit renderNeeded();
}

bool ClipSink::showArrow() const
{
  return m_showArrow;
}

void ClipSink::setShowArrow(bool show)
{
  m_showArrow = show;
  if (m_widget && m_widget->GetEnabled()) {
    m_widget->SetArrowVisibility(m_showPlane && show ? 1 : 0);
  }
  emit renderNeeded();
}

bool ClipSink::arrowVisible() const
{
  return m_widget && m_widget->GetEnabled() &&
         m_widget->GetArrowVisibility() != 0;
}

bool ClipSink::showPlane() const
{
  return m_showPlane;
}

void ClipSink::setShowPlane(bool show)
{
  m_showPlane = show;
  if (m_widget) {
    m_widget->SetTextureVisibility(show ? 1 : 0);
    // The widget ignores (and warns about) arrow changes while it is
    // off; setVisibility applies them when it comes back on
    if (m_widget->GetEnabled()) {
      m_widget->SetArrowVisibility(show && m_showArrow ? 1 : 0);
    }
  }
  emit renderNeeded();
}

void ClipSink::planeColor(double rgb[3]) const
{
  rgb[0] = m_planeColor[0];
  rgb[1] = m_planeColor[1];
  rgb[2] = m_planeColor[2];
}

void ClipSink::setPlaneColor(double r, double g, double b)
{
  m_planeColor[0] = r;
  m_planeColor[1] = g;
  m_planeColor[2] = b;
  if (m_widget) {
    m_widget->GetTexturePlaneProperty()->SetColor(r, g, b);
  }
  emit renderNeeded();
}

void ClipSink::planeCenter(double center[3]) const
{
  if (m_widget) {
    double* c = m_widget->GetCenter();
    center[0] = c[0];
    center[1] = c[1];
    center[2] = c[2];
  } else {
    double* o = m_clippingPlane->GetOrigin();
    center[0] = o[0];
    center[1] = o[1];
    center[2] = o[2];
  }
}

void ClipSink::planeNormal(double normal[3]) const
{
  double* n = m_clippingPlane->GetNormal();
  normal[0] = n[0];
  normal[1] = n[1];
  normal[2] = n[2];
}

int ClipSink::maxSlice() const
{
  int axis = directionAxis();
  if (axis < 0) {
    return -1;
  }
  return m_dims[axis] - 1;
}

int ClipSink::directionAxis() const
{
  switch (m_direction) {
    case XY: return 2;
    case YZ: return 0;
    case XZ: return 1;
    default: return -1;
  }
}

bool ClipSink::isOrtho() const
{
  return directionAxis() >= 0;
}

bool ClipSink::invertPlane() const
{
  return m_invertPlane;
}

void ClipSink::setInvertPlane(bool invert)
{
  if (m_invertPlane == invert) {
    return;
  }
  m_invertPlane = invert;

  // Flip the widget's normal
  if (m_widget) {
    double normal[3];
    m_widget->GetNormal(normal);
    normal[0] = -normal[0];
    normal[1] = -normal[1];
    normal[2] = -normal[2];
    m_widget->SetNormal(normal);
    m_widget->UpdatePlacement();
  }

  syncClippingPlane();
  emit clipPlaneUpdated();
  emit renderNeeded();
}

void ClipSink::setPlaneOrigin(double x, double y, double z)
{
  if (m_widget) {
    double c[3] = { x, y, z };
    m_widget->SetCenter(c);
  }
  syncClippingPlane();
  propagateToLinkedSinks();
  emit clipPlaneUpdated();
  emit renderNeeded();
}

void ClipSink::setPlaneNormal(double nx, double ny, double nz)
{
  if (m_widget) {
    double n[3] = { nx, ny, nz };
    m_widget->SetNormal(n);
  }
  syncClippingPlane();
  propagateToLinkedSinks();
  emit clipPlaneUpdated();
  emit renderNeeded();
}

void ClipSink::planeNormalInData(double normal[3]) const
{
  if (m_widget) {
    m_widget->GetNormal(normal);
    return;
  }

  double* n = m_clippingPlane->GetNormal();
  normal[0] = n[0];
  normal[1] = n[1];
  normal[2] = n[2];
}

void ClipSink::setPlaneDistance(double distance)
{
  double n[3];
  planeNormalInData(n);
  double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (length == 0.0) {
    return;
  }

  double boundsCenter[3] = { (m_bounds[0] + m_bounds[1]) / 2.0,
                             (m_bounds[2] + m_bounds[3]) / 2.0,
                             (m_bounds[4] + m_bounds[5]) / 2.0 };

  setPlaneOrigin(boundsCenter[0] + distance * n[0] / length,
                 boundsCenter[1] + distance * n[1] / length,
                 boundsCenter[2] + distance * n[2] / length);
}

void ClipSink::planeDistanceRange(double& minDistance,
                                  double& maxDistance) const
{
  double n[3];
  planeNormalInData(n);
  planeTravelRange(m_bounds, n, minDistance, maxDistance);
}

vtkPlane* ClipSink::clippingPlane() const
{
  return m_clippingPlane;
}

bool ClipSink::linked() const
{
  return m_linked;
}

void ClipSink::setLinked(bool linked)
{
  if (m_linked == linked) {
    return;
  }
  m_linked = linked;
  emit linkedChanged(m_linked);

  if (m_linked) {
    propagateToLinkedSinks();
  }
}

void ClipSink::propagateToLinkedSinks()
{
  if (!m_linked || s_propagatingClipLink) {
    return;
  }

  auto* pip = qobject_cast<Pipeline*>(parent());
  if (!pip) {
    return;
  }

  QScopedValueRollback<bool> guard(s_propagatingClipLink, true);
  for (auto* node : pip->nodes()) {
    auto* other = qobject_cast<ClipSink*>(node);
    if (!other || other == this || !other->linked()) {
      continue;
    }
    other->setDirection(m_direction);
    if (isOrtho()) {
      // Match by physical position so different voxel sizes line up;
      // fall back to the raw index until both geometries are known.
      if (!other->setSlicePosition(slicePosition())) {
        other->setSlice(m_slice);
      }
    } else if (m_widget) {
      // The widget holds the custom plane in data coordinates, which is
      // what the peer's setters take.
      double* n = m_widget->GetNormal();
      double* c = m_widget->GetCenter();
      other->setPlaneNormal(n[0], n[1], n[2]);
      other->setPlaneOrigin(c[0], c[1], c[2]);
    }
  }
}

double ClipSink::slicePosition() const
{
  int axis = directionAxis();
  if (axis < 0 || planeindex::spacing(m_dims, m_bounds, axis) <= 0.0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return planeindex::position(m_dims, m_bounds, axis, m_slice);
}

bool ClipSink::setSlicePosition(double position)
{
  int axis = directionAxis();
  if (axis < 0 || std::isnan(position)) {
    return false;
  }
  int index = planeindex::index(m_dims, m_bounds, axis, position);
  if (index < 0) {
    return false;
  }
  setSlice(index);
  return true;
}

void ClipSink::onWidgetInteractionStarted()
{
  if (!m_widget || !isOrtho()) {
    return;
  }
  // Grabbing the arrow (rotate) or the center sphere (move) asks for a
  // plane the axis-aligned directions cannot express: switch to Custom,
  // keeping the plane where it is.
  int state = m_widget->GetWidgetState();
  if (state == vtkNonOrthoImagePlaneWidget::Rotating ||
      state == vtkNonOrthoImagePlaneWidget::Moving) {
    setDirection(Custom);
  }
}

QJsonObject ClipSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["direction"] = static_cast<int>(m_direction);
  json["plane"] = m_slice;
  json["opacity"] = m_opacity;
  json["showPlane"] = m_showPlane;
  json["showArrow"] = m_showArrow;
  json["invertPlane"] = m_invertPlane;
  json["linked"] = m_linked;
  json["selectedColor"] = QJsonArray{ m_planeColor[0], m_planeColor[1],
                                      m_planeColor[2] };

  if (m_widget) {
    double origin[3], point1[3], point2[3];
    m_widget->GetOrigin(origin);
    m_widget->GetPoint1(point1);
    m_widget->GetPoint2(point2);
    json["origin"] = QJsonArray{ origin[0], origin[1], origin[2] };
    json["point1"] = QJsonArray{ point1[0], point1[1], point1[2] };
    json["point2"] = QJsonArray{ point2[0], point2[1], point2[2] };
  }

  return json;
}

bool ClipSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("direction")) {
    m_direction = static_cast<Direction>(json["direction"].toInt());
  }
  if (json.contains("plane")) {
    m_slice = json["plane"].toInt();
  }
  if (json.contains("opacity")) {
    setOpacity(json["opacity"].toDouble());
  }
  if (json.contains("showPlane")) {
    setShowPlane(json["showPlane"].toBool());
  }
  if (json.contains("showArrow")) {
    setShowArrow(json["showArrow"].toBool());
  }
  if (json.value("selectedColor").isArray()) {
    auto arr = json.value("selectedColor").toArray();
    if (arr.size() == 3) {
      setPlaneColor(arr.at(0).toDouble(), arr.at(1).toDouble(),
                    arr.at(2).toDouble());
    }
  }
  if (json.contains("linked")) {
    m_linked = json["linked"].toBool();
  }
  if (json.contains("invertPlane")) {
    setInvertPlane(json["invertPlane"].toBool());
  }
  if (m_widget && json.value("origin").isArray() &&
      json.value("point1").isArray() && json.value("point2").isArray()) {
    auto o = json.value("origin").toArray();
    auto p1 = json.value("point1").toArray();
    auto p2 = json.value("point2").toArray();
    if (o.size() == 3 && p1.size() == 3 && p2.size() == 3) {
      m_widget->SetOrigin(o.at(0).toDouble(), o.at(1).toDouble(),
                          o.at(2).toDouble());
      m_widget->SetPoint1(p1.at(0).toDouble(), p1.at(1).toDouble(),
                          p1.at(2).toDouble());
      m_widget->SetPoint2(p2.at(0).toDouble(), p2.at(1).toDouble(),
                          p2.at(2).toDouble());
      m_widget->UpdatePlacement();
    }
  }
  return true;
}

// --- Properties widget ---

QWidget* ClipSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* mainLayout = new QVBoxLayout(widget);
  auto* formLayout = new QFormLayout;
  mainLayout->addLayout(formLayout);

  // --- Opacity ---
  auto* opacitySlider = new DoubleSliderWidget(true, widget);
  opacitySlider->setLineEditWidth(50);
  opacitySlider->setMinimum(0);
  opacitySlider->setMaximum(1);
  opacitySlider->setValue(opacity());
  formLayout->addRow("Opacity", opacitySlider);
  connect(opacitySlider, &DoubleSliderWidget::valueEdited,
          [this](double v) { setOpacity(v); });
  connect(opacitySlider, &DoubleSliderWidget::valueChanged,
          [this](double v) { setOpacity(v); });

  // --- Plane Color ---
  auto* colorSelector = new pqColorChooserButton(widget);
  colorSelector->setShowAlphaChannel(false);
  colorSelector->setChosenColor(
    QColor::fromRgbF(m_planeColor[0], m_planeColor[1], m_planeColor[2]));
  formLayout->addRow("Select Color", colorSelector);
  connect(colorSelector, &pqColorChooserButton::chosenColorChanged,
          [this](const QColor& c) {
            setPlaneColor(c.redF(), c.greenF(), c.blueF());
          });

  // --- Show Plane / Show Arrow (horizontal row) ---
  auto* displayRowLayout = new QHBoxLayout;
  auto* showPlaneCheck = new QCheckBox("Show Plane", widget);
  {
    QSignalBlocker blocker(showPlaneCheck);
    showPlaneCheck->setChecked(showPlane());
  }
  displayRowLayout->addWidget(showPlaneCheck);

  auto* showArrowCheck = new QCheckBox("Show Arrow", widget);
  {
    QSignalBlocker blocker(showArrowCheck);
    showArrowCheck->setChecked(showArrow());
    showArrowCheck->setEnabled(showPlane());
  }
  displayRowLayout->addWidget(showArrowCheck);
  formLayout->addRow(displayRowLayout);

  connect(showPlaneCheck, &QCheckBox::toggled,
          [this, showArrowCheck](bool on) {
            setShowPlane(on);
            showArrowCheck->setEnabled(on);
          });
  connect(showArrowCheck, &QCheckBox::toggled,
          [this](bool on) { setShowArrow(on); });

  // --- Invert Plane Direction ---
  auto* invertCheck = new QCheckBox("Invert Plane Direction", widget);
  {
    QSignalBlocker blocker(invertCheck);
    invertCheck->setChecked(invertPlane());
  }
  formLayout->addRow(invertCheck);
  connect(invertCheck, &QCheckBox::toggled,
          [this](bool on) { setInvertPlane(on); });

  // --- Direction ---
  auto* dirCombo = new QComboBox(widget);
  dirCombo->addItem("XY Plane", static_cast<int>(XY));
  dirCombo->addItem("YZ Plane", static_cast<int>(YZ));
  dirCombo->addItem("XZ Plane", static_cast<int>(XZ));
  dirCombo->addItem("Custom", static_cast<int>(Custom));
  {
    QSignalBlocker blocker(dirCombo);
    dirCombo->setCurrentIndex(static_cast<int>(direction()));
  }
  formLayout->addRow("Direction", dirCombo);

  // --- Slice slider ---
  auto* sliceSlider = new IntSliderWidget(true, widget);
  sliceSlider->setLineEditWidth(50);
  sliceSlider->setPageStep(1);
  sliceSlider->setMinimum(0);
  {
    int ms = maxSlice();
    if (ms >= 0) {
      sliceSlider->setMaximum(ms);
    }
    int s = slice();
    if (s < sliceSlider->minimum()) {
      s = sliceSlider->minimum();
    } else if (s > sliceSlider->maximum()) {
      s = sliceSlider->maximum();
    }
    sliceSlider->setValue(s);
  }
  sliceSlider->setVisible(isOrtho());
  formLayout->addRow("Plane", sliceSlider);
  connect(sliceSlider, &IntSliderWidget::valueEdited,
          [this](int v) { setSlice(v); });
  connect(sliceSlider, &IntSliderWidget::valueChanged,
          [this](int v) {
            m_slice = v;
            if (m_widget && isOrtho()) {
              int axis = directionAxis();
              m_slice = qBound(0, m_slice, m_dims[axis] - 1);
              m_widget->SetSliceIndex(m_slice);
              onWidgetInteraction();
              if (m_widget->GetInteractor()) {
                m_widget->GetInteractor()->Render();
              }
            }
          });

  // --- Link to other clips ---
  auto* linkCheck = new QCheckBox(widget);
  linkCheck->setToolTip(
    "Move every linked clip together. Turn this on in two or more clips "
    "(for example one per element of a simultaneously acquired dataset) and "
    "changing the direction or plane in any of them applies the same to the "
    "others.");
  {
    QSignalBlocker blocker(linkCheck);
    linkCheck->setChecked(linked());
  }
  formLayout->addRow("Link Clips", linkCheck);
  connect(linkCheck, &QCheckBox::toggled,
          [this](bool on) { setLinked(on); });
  connect(this, &ClipSink::linkedChanged, linkCheck, [linkCheck](bool on) {
    QSignalBlocker blocker(linkCheck);
    linkCheck->setChecked(on);
  });

  // --- Separator ---
  auto* line = new QFrame(widget);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  formLayout->addRow(line);

  // --- Point on Plane ---
  double center[3], normal[3];
  planeCenter(center);
  planeNormal(normal);
  bool ortho = isOrtho();

  QLineEdit* pointInputs[3];
  QLineEdit* normalInputs[3];

  mainLayout->addWidget(new QLabel("Point on Plane", widget));
  auto* pointRow = new QHBoxLayout;
  const char* labels[] = { "X:", "Y:", "Z:" };
  for (int i = 0; i < 3; ++i) {
    pointRow->addWidget(new QLabel(labels[i], widget));
    auto* edit = new QLineEdit(QString::number(center[i]), widget);
    edit->setValidator(new QDoubleValidator(edit));
    edit->setEnabled(!ortho);
    pointRow->addWidget(edit);
    pointInputs[i] = edit;
  }
  mainLayout->addLayout(pointRow);

  // --- Plane Normal ---
  mainLayout->addWidget(new QLabel("Plane Normal", widget));
  auto* normalRow = new QHBoxLayout;
  for (int i = 0; i < 3; ++i) {
    normalRow->addWidget(new QLabel(labels[i], widget));
    auto* edit = new QLineEdit(QString::number(normal[i]), widget);
    edit->setValidator(new QDoubleValidator(edit));
    edit->setEnabled(!ortho);
    normalRow->addWidget(edit);
    normalInputs[i] = edit;
  }
  mainLayout->addLayout(normalRow);

  // --- Set Normal to View button ---
  auto* normalToViewButton = new QPushButton("Set Normal to View", widget);
  normalToViewButton->setToolTip("Set the plane normal to the view direction");
  mainLayout->addWidget(normalToViewButton);

  mainLayout->addStretch();

  // --- Signal connections ---

  // Point/Normal manual editing
  auto updateCenterFn = [this, pointInputs]() {
    setPlaneOrigin(pointInputs[0]->text().toDouble(),
                   pointInputs[1]->text().toDouble(),
                   pointInputs[2]->text().toDouble());
  };
  auto updateNormalFn = [this, normalInputs]() {
    setPlaneNormal(normalInputs[0]->text().toDouble(),
                   normalInputs[1]->text().toDouble(),
                   normalInputs[2]->text().toDouble());
  };
  for (int i = 0; i < 3; ++i) {
    connect(pointInputs[i], &QLineEdit::editingFinished, updateCenterFn);
    connect(normalInputs[i], &QLineEdit::editingFinished, updateNormalFn);
  }

  // Set Normal to View
  connect(normalToViewButton, &QPushButton::clicked, this,
          [this, dirCombo, normalInputs]() {
            if (direction() != Custom) {
              setDirection(Custom);
              dirCombo->setCurrentIndex(static_cast<int>(Custom));
            }
            auto* rv = renderView();
            if (!rv) {
              return;
            }
            auto* cam = rv->GetActiveCamera();
            double* pos = cam->GetPosition();
            double* fp = cam->GetFocalPoint();
            double n[3] = { fp[0] - pos[0], fp[1] - pos[1], fp[2] - pos[2] };
            setPlaneNormal(n[0], n[1], n[2]);
            for (int i = 0; i < 3; ++i) {
              QSignalBlocker b(normalInputs[i]);
              normalInputs[i]->setText(QString::number(n[i]));
            }
          });

  // Follow direction changes made from the sink itself (arrow drag,
  // linked peer); the combo handler below then updates the rest.
  connect(this, &ClipSink::directionChanged, widget,
          [dirCombo](Direction dir) {
            int idx = dirCombo->findData(static_cast<int>(dir));
            if (idx >= 0 && idx != dirCombo->currentIndex()) {
              dirCombo->setCurrentIndex(idx);
            }
          });

  // Direction combo
  connect(dirCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this, sliceSlider, pointInputs, normalInputs, dirCombo](int idx) {
            auto dir =
              static_cast<Direction>(dirCombo->itemData(idx).toInt());
            setDirection(dir);
            bool isOrthoDir = (dir != Custom);
            sliceSlider->setVisible(isOrthoDir);
            for (int i = 0; i < 3; ++i) {
              pointInputs[i]->setEnabled(!isOrthoDir);
              normalInputs[i]->setEnabled(!isOrthoDir);
            }
            if (isOrthoDir) {
              int ms = maxSlice();
              sliceSlider->setMinimum(0);
              sliceSlider->setMaximum(ms >= 0 ? ms : 0);
              sliceSlider->setValue(slice() >= 0 ? slice() : 0);
            }
          });

  // Update slice slider when the widget is dragged in 3D
  connect(this, &ClipSink::sliceChanged, widget, [sliceSlider](int s) {
    QSignalBlocker blocker(sliceSlider);
    sliceSlider->setValue(s);
  });

  // Update slider range after pipeline execution (m_dims may not be
  // populated when the properties widget is first created, because
  // nodeAdded fires before execute()).
  connect(this, &ClipSink::execStateChanged, widget,
          [this, sliceSlider](NodeExecState state) {
            if (state == NodeExecState::Idle && isOrtho()) {
              int ms = maxSlice();
              if (ms >= 0) {
                sliceSlider->setMaximum(ms);
              }
              int s = slice();
              QSignalBlocker blocker(sliceSlider);
              sliceSlider->setValue(qBound(0, s, ms));
            }
          });

  // Update point/normal text inputs when the plane is dragged interactively
  connect(this, &ClipSink::clipPlaneUpdated, widget,
          [this, pointInputs, normalInputs]() {
            double c[3], n[3];
            planeCenter(c);
            planeNormal(n);
            for (int i = 0; i < 3; ++i) {
              QSignalBlocker pb(pointInputs[i]);
              pointInputs[i]->setText(QString::number(c[i]));
              QSignalBlocker nb(normalInputs[i]);
              normalInputs[i]->setText(QString::number(n[i]));
            }
          });

  return widget;
}

// --- Widget interaction → clipping plane sync ---

void ClipSink::syncClippingPlane()
{
  if (!m_widget) {
    return;
  }

  // The widget center/normal are in data coordinates.  Sibling sinks'
  // mappers clip in world coordinates, so transform by the volume's
  // display position and orientation (same transform the actors use).
  double center[3], normal[3];
  m_widget->GetCenter(center);
  m_widget->GetNormal(normal);

  auto vol = volumeData();
  if (vol) {
    auto pos = vol->displayPosition();
    auto orient = vol->displayOrientation();

    vtkNew<vtkTransform> t;
    t->Translate(pos.data());
    t->RotateZ(orient[2]);
    t->RotateX(orient[0]);
    t->RotateY(orient[1]);

    t->TransformPoint(center, center);
    t->TransformNormal(normal, normal);
  }

  m_clippingPlane->SetOrigin(center);
  m_clippingPlane->SetNormal(normal);
}

void ClipSink::onWidgetInteraction()
{
  if (!m_widget) {
    return;
  }

  syncClippingPlane();

  // For orthogonal directions, update the slice index from the widget
  if (isOrtho()) {
    int axis = directionAxis();
    int newSlice = planeindex::index(m_dims, m_bounds, axis,
                                     m_widget->GetCenter()[axis]);
    if (newSlice >= 0 && newSlice != m_slice) {
      m_slice = newSlice;
      emit sliceChanged(m_slice);
    }
  } else {
    propagateToLinkedSinks();
  }

  emit clipPlaneUpdated();
  emit renderNeeded();
}

// --- Clipping plane propagation to sibling sinks ---

void ClipSink::onInputConnectionChanged()
{
  disconnectFromSiblings();

  auto* input = inputPort("volume");
  if (!input || !input->link()) {
    return;
  }

  m_upstreamPort = input->link()->from();
  connectToSiblings();
}

void ClipSink::connectToSiblings()
{
  if (!m_upstreamPort) {
    return;
  }

  for (auto* sink : siblingSinks("volume")) {
    m_clippedSinks.insert(sink);
    if (visibility()) {
      sink->addClippingPlane(m_clippingPlane);
    }
  }

  auto* pip = qobject_cast<Pipeline*>(parent());
  if (pip) {
    connect(pip, &Pipeline::linkCreated,
            this, &ClipSink::onPipelineLinkCreated);
    connect(pip, &Pipeline::linkRemoved,
            this, &ClipSink::onPipelineLinkRemoved);
  }
}

void ClipSink::disconnectFromSiblings()
{
  for (auto* sink : m_clippedSinks) {
    sink->removeClippingPlane(m_clippingPlane);
  }
  m_clippedSinks.clear();
  m_upstreamPort = nullptr;

  auto* pip = qobject_cast<Pipeline*>(parent());
  if (pip) {
    disconnect(pip, &Pipeline::linkCreated,
               this, &ClipSink::onPipelineLinkCreated);
    disconnect(pip, &Pipeline::linkRemoved,
               this, &ClipSink::onPipelineLinkRemoved);
  }
}

void ClipSink::onPipelineLinkCreated(Link* link)
{
  if (!link || link->from() != m_upstreamPort) {
    return;
  }
  auto* sink = qobject_cast<LegacyModuleSink*>(link->to()->node());
  if (sink && sink != this && !m_clippedSinks.contains(sink)) {
    m_clippedSinks.insert(sink);
    if (visibility()) {
      sink->addClippingPlane(m_clippingPlane);
    }
  }
}

void ClipSink::onPipelineLinkRemoved(Link* link)
{
  if (!link || link->from() != m_upstreamPort) {
    return;
  }
  auto* sink = qobject_cast<LegacyModuleSink*>(link->to()->node());
  if (sink && m_clippedSinks.contains(sink)) {
    sink->removeClippingPlane(m_clippingPlane);
    m_clippedSinks.remove(sink);
  }
}

void ClipSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol || !m_widget) {
    return;
  }

  auto pos = vol->displayPosition();
  auto orient = vol->displayOrientation();
  m_widget->SetDisplayOffset(pos.data());
  m_widget->SetDisplayOrientation(orient.data());

  // Spacing changes require re-setting the plane orientation to resize the
  // plane geometry to the new physical extent.  Preserve the plane's relative
  // position so it doesn't jump to the midpoint.
  if (vol->isValid()) {
    auto sp = vol->spacing();
    if (sp != m_lastSpacing) {
      double center[3];
      m_widget->GetCenter(center);
      for (int i = 0; i < 3; ++i) {
        if (m_lastSpacing[i] != 0.0) {
          center[i] *= sp[i] / m_lastSpacing[i];
        }
      }
      m_lastSpacing = sp;
      m_widget->SetPlaneOrientation(m_widget->GetPlaneOrientation());
      m_widget->SetCenter(center);
      m_widget->UpdatePlacement();
    }
  }

  syncClippingPlane();
  emit clipPlaneUpdated();
  emit renderNeeded();
}

} // namespace pipeline
} // namespace tomviz
