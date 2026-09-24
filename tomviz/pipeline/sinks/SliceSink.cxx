/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SliceSink.h"

#include "ActiveObjects.h"
#include "ClipSink.h"
#include "DoubleSliderWidget.h"
#include "IntSliderWidget.h"
#include "Node.h"
#include "Pipeline.h"
#include "data/VolumeData.h"
#include "vtkActiveScalarsProducer.h"
#include "PlaneIndexing.h"
#include "vtkNonOrthoImagePlaneWidget.h"

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
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <limits>

#include <pqCoreUtilities.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCommand.h>
#include <vtkCamera.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkPVRenderView.h>
#include <vtkPointData.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkScalarsToColors.h>
#include <vtkSMProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>

namespace tomviz {
namespace pipeline {

namespace {

// Set while a linked slice pushes its state onto its peers, so their
// change signals do not echo it back. GUI thread only.
bool s_propagatingSliceLink = false;

} // namespace

SliceSink::SliceSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Slice");

  connect(this, &SliceSink::sliceChanged,
          this, &SliceSink::propagateToLinkedSinks);
  connect(this, &SliceSink::directionChanged,
          this, &SliceSink::propagateToLinkedSinks);
}

SliceSink::~SliceSink()
{
  finalize();
}

QIcon SliceSink::icon() const
{
  return QIcon(QStringLiteral(":/icons/orthoslice.svg"));
}

void SliceSink::setVisibility(bool visible)
{
  if (m_widget && m_dataReceived) {
    m_widget->SetEnabled(visible ? 1 : 0);
    if (visible) {
      // Widget methods that mutate arrow / interaction state warn when
      // the widget isn't enabled; re-push the cached values now that
      // we've just enabled it.
      m_widget->SetInteraction(m_showArrow ? 1 : 0);
      m_widget->SetArrowVisibility(m_showArrow ? 1 : 0);
    }
  }
  LegacyModuleSink::setVisibility(visible);
}

bool SliceSink::isColorMapNeeded() const
{
  return true;
}

void SliceSink::setupWidget()
{
  m_widget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();

  // Default: red border
  double color[3] = { 1, 0, 0 };
  m_widget->GetPlaneProperty()->SetColor(color);

  // Default: nearest-neighbor interpolation (matching legacy ModuleSlice)
  m_widget->TextureInterpolateOff();
  m_widget->SetResliceInterpolateToNearestNeighbour();

  // Apply current state (arrow visibility is deferred until after
  // SetInteractor + On, since the widget warns otherwise).
  m_widget->SetThickSliceMode(static_cast<int>(m_thickSliceMode));
  m_widget->SetOpacity(m_opacity);
  m_widget->SetMapScalars(m_mapScalars);

  if (m_interpolate) {
    m_widget->SetTextureInterpolate(1);
    m_widget->SetResliceInterpolate(VTK_LINEAR_RESLICE);
  } else {
    m_widget->SetTextureInterpolate(0);
    m_widget->SetResliceInterpolate(VTK_NEAREST_RESLICE);
  }
}

bool SliceSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  vtkRenderWindowInteractor* rwi = view->GetRenderWindow()->GetInteractor();
  if (!rwi) {
    return false;
  }

  setupWidget();
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

  // Turn On() briefly so the widget creates its texture object, then
  // immediately Off() so the placeholder geometry (with its oversized
  // arrow) is never visible. consume() will re-enable the widget once
  // real data arrives.
  m_widget->On();
  m_widget->Off();

  // When the user drags the slice in the 3D view, update our state and UI.
  pqCoreUtilities::connect(m_widget, vtkCommand::InteractionEvent, this,
                           SLOT(onPlaneChanged()));
  pqCoreUtilities::connect(m_widget, vtkCommand::StartInteractionEvent, this,
                           SLOT(onInteractionStarted()));

  // Report the voxel under the cursor (the widget picks it on a short
  // timer while the mouse moves over the slice); the main window shows
  // it in the status bar.
  m_widget->SetVoxelValueFn([](const vtkVector3i& ijk, double value) {
    emit ActiveObjects::instance().mouseOverVoxel(ijk, value);
  });

  return true;
}

bool SliceSink::finalize()
{
  if (m_widget) {
    if (m_widget->GetInteractor() && m_widget->GetEnabled()) {
      m_widget->InteractionOff();
      m_widget->Off();
    }
    m_widget->SetInteractor(nullptr);
    m_widget = nullptr;
  }
  return LegacyModuleSink::finalize();
}

void SliceSink::clearVisualization()
{
  if (m_widget && m_widget->GetInteractor()) {
    m_widget->Off();
  }
}

bool SliceSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  auto dims = volume->dimensions();
  m_dims[0] = dims[0];
  m_dims[1] = dims[1];
  m_dims[2] = dims[2];
  volume->imageData()->GetBounds(m_bounds);

  if (m_widget) {
    // Feed data to the widget via the active-scalars producer so it can
    // independently select which scalar array to display.
    m_producer->SetOutput(volume->imageData());
    applyActiveScalars();
    m_widget->SetInputConnection(m_producer->GetOutputPort());

    // Apply direction and slice
    applyDirection();

    // Apply thick slicing
    m_widget->SetSliceThickness(m_sliceThickness);

    // SetEnabled() touches the renderer and Qt GL state. consume() runs
    // on the GUI thread (consumeOnGuiThread()), so apply directly.
    m_dataReceived = true;
    m_widget->SetEnabled(visibility() ? 1 : 0);
    if (visibility()) {
      m_widget->SetArrowVisibility(m_showArrow ? 1 : 0);
      m_widget->SetInteraction(m_showArrow ? 1 : 0);
    }
  } else {
    // No widget yet (no view initialized) — just store dimensions for later
    if (m_slice < 0 && isOrtho()) {
      m_slice = m_dims[directionAxis()] / 2;
    }
  }

  // Notify the UI of the current state. These are sync notifications,
  // not user edits: hold the link guard so an execution never
  // re-propagates a peer's clamped index back onto wider datasets.
  {
    QScopedValueRollback<bool> guard(s_propagatingSliceLink, true);
    emit directionChanged(m_direction);
    emit sliceChanged(m_slice);
    emit planeChanged();
  }

  auto vol = volumeData();
  if (vol && vol->isValid()) {
    m_lastSpacing = vol->spacing();
    m_lastOrigin = vol->origin();
  }

  onMetadataChanged();
  emit renderNeeded();
  return true;
}

void SliceSink::applyDirection()
{
  if (!m_widget) {
    return;
  }

  int axis = directionAxis();

  if (axis >= 0) {
    // Orthogonal direction
    m_widget->SetPlaneOrientation(axis);

    // Default to mid-slice if not explicitly set
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
    if (m_planeCenterSet) {
      m_widget->SetCenter(m_planeCenter);
      m_widget->UpdatePlacement();
    }
  }
}

int SliceSink::directionAxis() const
{
  switch (m_direction) {
    case XY:
      return 2;
    case YZ:
      return 0;
    case XZ:
      return 1;
    default:
      return -1;
  }
}

SliceSink::Direction SliceSink::direction() const
{
  return m_direction;
}

void SliceSink::setDirection(Direction dir)
{
  if (m_direction == dir) {
    return;
  }
  m_direction = dir;

  if (isOrtho() && m_dims[directionAxis()] > 0) {
    double normal[3] = { 0, 0, 0 };
    int axis = directionAxis();
    normal[axis] = 1;
    m_planeNormal[0] = normal[0];
    m_planeNormal[1] = normal[1];
    m_planeNormal[2] = normal[2];
    m_slice = m_dims[axis] / 2;
  }

  applyDirection();
  emit directionChanged(dir);
  emit renderNeeded();
}

bool SliceSink::isOrtho() const
{
  return directionAxis() >= 0;
}

int SliceSink::slice() const
{
  return m_slice;
}

void SliceSink::setSlice(int index)
{
  m_slice = index;
  if (m_widget && isOrtho()) {
    int axis = directionAxis();
    m_slice = qBound(0, m_slice, m_dims[axis] - 1);
    m_widget->SetSliceIndex(m_slice);
  }
  emit sliceChanged(m_slice);
  emit renderNeeded();
}

int SliceSink::maxSlice() const
{
  if (!isOrtho()) {
    return -1;
  }
  int axis = directionAxis();
  return m_dims[axis] - 1;
}

double SliceSink::opacity() const
{
  return m_opacity;
}

void SliceSink::setOpacity(double value)
{
  m_opacity = value;
  if (m_widget) {
    m_widget->SetOpacity(value);
  }
  emit renderNeeded();
}

int SliceSink::sliceThickness() const
{
  return m_sliceThickness;
}

void SliceSink::setSliceThickness(int slices)
{
  m_sliceThickness = slices;
  if (m_widget) {
    m_widget->SetSliceThickness(slices);
  }
  emit renderNeeded();
}

SliceSink::ThickSliceMode SliceSink::thickSliceMode() const
{
  return m_thickSliceMode;
}

void SliceSink::setThickSliceMode(ThickSliceMode mode)
{
  m_thickSliceMode = mode;
  if (m_widget) {
    m_widget->SetThickSliceMode(static_cast<int>(mode));
  }
  emit renderNeeded();
}

bool SliceSink::textureInterpolate() const
{
  return m_interpolate;
}

void SliceSink::setTextureInterpolate(bool interpolate)
{
  m_interpolate = interpolate;
  if (m_widget) {
    int val = interpolate ? 1 : 0;
    m_widget->SetTextureInterpolate(val);
    m_widget->SetResliceInterpolate(val);
  }
  emit renderNeeded();
}

bool SliceSink::showArrow() const
{
  return m_showArrow;
}

void SliceSink::setShowArrow(bool show)
{
  m_showArrow = show;
  // SetArrowVisibility / SetInteraction warn if called on a disabled
  // widget. Defer them to the next setVisibility(true), which re-pushes
  // m_showArrow to the widget.
  if (m_widget && m_widget->GetEnabled()) {
    m_widget->SetArrowVisibility(show ? 1 : 0);
    // Interaction follows arrow visibility
    if (visibility()) {
      m_widget->SetInteraction(show ? 1 : 0);
    }
  }
  emit renderNeeded();
}

bool SliceSink::mapScalars() const
{
  return m_mapScalars;
}

void SliceSink::setMapScalars(bool map)
{
  m_mapScalars = map;
  if (m_widget) {
    m_widget->SetMapScalars(map);
  }
  emit renderNeeded();
}

int SliceSink::activeScalars() const
{
  return m_activeScalars;
}

void SliceSink::setActiveScalars(int index)
{
  m_activeScalars = index;
  applyActiveScalars();
  emit renderNeeded();
}

bool SliceSink::linked() const
{
  return m_linked;
}

void SliceSink::setLinked(bool linked)
{
  if (m_linked == linked) {
    return;
  }
  m_linked = linked;
  emit linkedChanged(m_linked);

  if (m_linked) {
    // Adopt this view's state everywhere as soon as linking turns on
    propagateToLinkedSinks();
  }
}

void SliceSink::propagateToLinkedSinks()
{
  if (!m_linked || s_propagatingSliceLink) {
    return;
  }

  auto* pip = qobject_cast<Pipeline*>(parent());
  if (!pip) {
    return;
  }

  QScopedValueRollback<bool> guard(s_propagatingSliceLink, true);
  for (auto* node : pip->nodes()) {
    auto* other = qobject_cast<SliceSink*>(node);
    if (!other || other == this || !other->linked()) {
      continue;
    }
    // Direction first: changing it re-centers the peer's slice, which
    // the position below then overwrites.
    other->setDirection(m_direction);
    if (isOrtho()) {
      // Match by physical position so different voxel sizes line up;
      // fall back to the raw index until both geometries are known.
      if (!other->setSlicePosition(slicePosition())) {
        other->setSlice(m_slice);
      }
    } else {
      double c[3], n[3];
      planeCenter(c);
      planeNormal(n);
      other->setPlaneNormal(n[0], n[1], n[2]);
      other->setPlaneCenter(c[0], c[1], c[2]);
    }
  }
}

double SliceSink::slicePosition() const
{
  int axis = directionAxis();
  if (axis < 0 || planeindex::spacing(m_dims, m_bounds, axis) <= 0.0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return planeindex::position(m_dims, m_bounds, axis, m_slice);
}

bool SliceSink::setSlicePosition(double position)
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

void SliceSink::onInteractionStarted()
{
  if (!m_widget || !isOrtho()) {
    return;
  }
  // Grabbing the arrow (rotate) or the center sphere (move) is asking
  // for a plane the axis-aligned directions cannot express, so switch
  // to Custom from the plane's current placement.
  int state = m_widget->GetWidgetState();
  if (state != vtkNonOrthoImagePlaneWidget::Rotating &&
      state != vtkNonOrthoImagePlaneWidget::Moving) {
    return;
  }
  m_widget->GetCenter(m_planeCenter);
  m_widget->GetNormal(m_planeNormal);
  m_planeCenterSet = true;
  setDirection(Custom);
}

void SliceSink::applyActiveScalars()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  resolvePendingActiveScalar(m_activeScalars);

  auto* pointData = vol->imageData()->GetPointData();
  vtkDataArray* selected = nullptr;
  int idx = m_activeScalars;
  if (idx >= 0 && idx < pointData->GetNumberOfArrays()) {
    selected = pointData->GetArray(idx);
  }
  if (!selected) {
    // Default or saved index doesn't fit the current data — fall back
    // to active. m_activeScalars left alone.
    selected = pointData->GetScalars();
  }
  if (selected && selected->GetName()) {
    m_producer->SetActiveScalars(selected->GetName());
  }
}

void SliceSink::updateColorMap()
{
  if (!m_widget) {
    return;
  }
  auto* cmap = colorMap();
  if (cmap) {
    auto* ctf = vtkScalarsToColors::SafeDownCast(
      cmap->GetClientSideObject());
    if (ctf) {
      m_widget->SetLookupTable(ctf);
    }
  }
  emit renderNeeded();
}

// --- Clipping ---

void SliceSink::addClippingPlane(vtkPlane* plane)
{
  if (plane && m_widget) {
    m_widget->GetResliceMapper(plane, false);
    emit renderNeeded();
  }
}

void SliceSink::removeClippingPlane(vtkPlane* plane)
{
  if (plane && m_widget) {
    m_widget->GetResliceMapper(plane, true);
    emit renderNeeded();
  }
}

void SliceSink::onPlaneChanged()
{
  if (!m_widget) {
    return;
  }

  // Read current widget state
  double* center = m_widget->GetCenter();
  double* normal = m_widget->GetNormal();

  // Cache the plane geometry so the UI can read it and so that
  // applyDirection() can restore it if the pipeline re-executes.
  for (int i = 0; i < 3; ++i) {
    m_planeCenter[i] = center[i];
    m_planeNormal[i] = normal[i];
  }
  m_planeCenterSet = true;

  // For orthogonal directions, update the slice index from the widget
  if (isOrtho()) {
    int axis = directionAxis();
    int newSlice = planeindex::index(m_dims, m_bounds, axis, center[axis]);
    if (newSlice >= 0 && newSlice != m_slice) {
      m_slice = newSlice;
      emit sliceChanged(m_slice);
    }
  } else {
    propagateToLinkedSinks();
  }

  emit planeChanged();
  emit renderNeeded();
}

void SliceSink::setPlaneCenter(double x, double y, double z)
{
  m_planeCenter[0] = x;
  m_planeCenter[1] = y;
  m_planeCenter[2] = z;
  m_planeCenterSet = true;
  if (m_widget) {
    m_widget->SetCenter(m_planeCenter);
    m_widget->UpdatePlacement();
  }
  propagateToLinkedSinks();
  emit renderNeeded();
}

void SliceSink::planeCenter(double xyz[3]) const
{
  if (m_widget) {
    m_widget->GetCenter(xyz);
  } else {
    xyz[0] = m_planeCenter[0];
    xyz[1] = m_planeCenter[1];
    xyz[2] = m_planeCenter[2];
  }
}

void SliceSink::setPlaneNormal(double x, double y, double z)
{
  m_planeNormal[0] = x;
  m_planeNormal[1] = y;
  m_planeNormal[2] = z;
  if (m_widget) {
    m_widget->SetNormal(m_planeNormal);
    m_widget->UpdatePlacement();
  }
  propagateToLinkedSinks();
  emit renderNeeded();
}

void SliceSink::planeNormal(double xyz[3]) const
{
  if (m_widget) {
    m_widget->GetNormal(xyz);
  } else {
    xyz[0] = m_planeNormal[0];
    xyz[1] = m_planeNormal[1];
    xyz[2] = m_planeNormal[2];
  }
}

double SliceSink::planeDistance() const
{
  double n[3], c[3];
  planeNormal(n);
  planeCenter(c);
  double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (length == 0.0) {
    return 0.0;
  }
  double distance = 0.0;
  for (int i = 0; i < 3; ++i) {
    double mid = (m_bounds[2 * i] + m_bounds[2 * i + 1]) / 2.0;
    distance += (c[i] - mid) * n[i] / length;
  }
  return distance;
}

void SliceSink::setPlaneDistance(double distance)
{
  double n[3];
  planeNormal(n);
  double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (length == 0.0) {
    return;
  }
  setPlaneCenter((m_bounds[0] + m_bounds[1]) / 2.0 + distance * n[0] / length,
                 (m_bounds[2] + m_bounds[3]) / 2.0 + distance * n[1] / length,
                 (m_bounds[4] + m_bounds[5]) / 2.0 + distance * n[2] / length);
}

void SliceSink::planeDistanceRange(double& minDistance,
                                   double& maxDistance) const
{
  double n[3];
  planeNormal(n);
  planeTravelRange(m_bounds, n, minDistance, maxDistance);
}

QWidget* SliceSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* mainLayout = new QVBoxLayout(widget);
  auto* formLayout = new QFormLayout;
  mainLayout->addLayout(formLayout);

  // --- Color Map Data ---
  auto* mapCheck = new QCheckBox(widget);
  {
    QSignalBlocker blocker(mapCheck);
    mapCheck->setChecked(mapScalars());
  }
  formLayout->addRow("Color Map Data", mapCheck);
  connect(mapCheck, &QCheckBox::toggled,
          [this](bool on) { setMapScalars(on); });

  // --- Separate Color Map ---
  auto* separateCmapCheck = new QCheckBox(widget);
  {
    QSignalBlocker blocker(separateCmapCheck);
    separateCmapCheck->setChecked(useDetachedColorMap());
  }
  formLayout->addRow("Separate Color Map", separateCmapCheck);
  connect(separateCmapCheck, &QCheckBox::toggled,
          [this](bool on) { setUseDetachedColorMap(on); });

  // --- Horizontal divider ---
  auto* line = new QFrame(widget);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  formLayout->addRow(line);

  // --- Active Scalars ---
  auto* scalarsCombo = new QComboBox(widget);
  {
    QSignalBlocker blocker(scalarsCombo);
    scalarsCombo->addItem("Default", -1);
    auto vol = volumeData();
    if (vol && vol->isValid()) {
      auto* pointData = vol->imageData()->GetPointData();
      for (int i = 0; i < pointData->GetNumberOfArrays(); ++i) {
        auto* array = pointData->GetArray(i);
        if (array && array->GetName()) {
          scalarsCombo->addItem(QString(array->GetName()), i);
        }
      }
    }
    if (m_activeScalars < 0) {
      scalarsCombo->setCurrentIndex(0);
    } else {
      int idx = scalarsCombo->findData(m_activeScalars);
      scalarsCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
  }
  formLayout->addRow("Scalars", scalarsCombo);
  connect(scalarsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this, scalarsCombo](int idx) {
            setActiveScalars(scalarsCombo->itemData(idx).toInt());
          });

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
  formLayout->addRow("Slice", sliceSlider);

  // --- Link to other slice views ---
  auto* linkCheck = new QCheckBox(widget);
  linkCheck->setToolTip(
    "Step through every linked slice view together. Turn this on in two or "
    "more slice views (for example one per element of a simultaneously "
    "acquired dataset) and changing the direction or slice in any of them "
    "applies the same to the others.");
  {
    QSignalBlocker blocker(linkCheck);
    linkCheck->setChecked(linked());
  }
  formLayout->addRow("Link Slices", linkCheck);
  connect(linkCheck, &QCheckBox::toggled,
          [this](bool on) { setLinked(on); });
  connect(this, &SliceSink::linkedChanged, linkCheck, [linkCheck](bool on) {
    QSignalBlocker blocker(linkCheck);
    linkCheck->setChecked(on);
  });
  // valueEdited fires on release / text commit → full-quality render
  connect(sliceSlider, &IntSliderWidget::valueEdited,
          [this](int v) { setSlice(v); });
  // valueChanged fires on every drag tick → fast interactive render
  connect(sliceSlider, &IntSliderWidget::valueChanged,
          [this](int v) {
            m_slice = v;
            if (m_widget && isOrtho()) {
              int axis = directionAxis();
              m_slice = qBound(0, m_slice, m_dims[axis] - 1);
              m_widget->SetSliceIndex(m_slice);
              if (m_widget->GetInteractor()) {
                m_widget->GetInteractor()->Render();
              }
            }
          });

  // --- Slice Thickness ---
  auto* thickSpin = new QSpinBox(widget);
  thickSpin->setMinimum(1);
  {
    int ms = maxSlice();
    thickSpin->setMaximum(ms > 0 ? ms : 999);
  }
  thickSpin->setSingleStep(2);
  {
    QSignalBlocker blocker(thickSpin);
    thickSpin->setValue(sliceThickness());
  }
  formLayout->addRow("Slice Thickness", thickSpin);

  // --- Aggregation ---
  auto* aggCombo = new QComboBox(widget);
  aggCombo->addItem("Minimum", static_cast<int>(Min));
  aggCombo->addItem("Maximum", static_cast<int>(Max));
  aggCombo->addItem("Mean", static_cast<int>(Mean));
  aggCombo->addItem("Summation", static_cast<int>(Sum));
  {
    QSignalBlocker blocker(aggCombo);
    aggCombo->setCurrentIndex(static_cast<int>(thickSliceMode()));
  }
  formLayout->addRow("Aggregation", aggCombo);

  connect(thickSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          [this](int v) { setSliceThickness(v); });
  connect(aggCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this, aggCombo](int idx) {
            setThickSliceMode(
              static_cast<ThickSliceMode>(aggCombo->itemData(idx).toInt()));
          });

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

  // --- Interpolate Texture ---
  auto* interpCheck = new QCheckBox(widget);
  {
    QSignalBlocker blocker(interpCheck);
    interpCheck->setChecked(textureInterpolate());
  }
  formLayout->addRow("Interpolate Texture", interpCheck);
  connect(interpCheck, &QCheckBox::toggled,
          [this](bool on) { setTextureInterpolate(on); });

  // --- Show Arrow ---
  auto* arrowCheck = new QCheckBox(widget);
  {
    QSignalBlocker blocker(arrowCheck);
    arrowCheck->setChecked(showArrow());
  }
  formLayout->addRow("Show Arrow", arrowCheck);
  connect(arrowCheck, &QCheckBox::toggled,
          [this](bool on) { setShowArrow(on); });

  // --- Point on Plane ---
  double center[3], normal[3];
  planeCenter(center);
  planeNormal(normal);

  QLineEdit* pointInputs[3];
  QLineEdit* normalInputs[3];
  bool ortho = isOrtho();

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

  // --- Set Normal to View ---
  auto* normalToViewButton = new QPushButton("Set Normal to View", widget);
  normalToViewButton->setToolTip("Set the plane normal to the view direction");
  mainLayout->addWidget(normalToViewButton);

  mainLayout->addStretch();

  // --- Signal connections ---

  // Point/Normal editing
  auto updateCenterFn = [this, pointInputs]() {
    setPlaneCenter(pointInputs[0]->text().toDouble(),
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

  // Set Normal to View button
  connect(normalToViewButton, &QPushButton::clicked, this,
          [this, dirCombo, normalInputs]() {
            // Switch to custom direction if needed
            if (direction() != Custom) {
              setDirection(Custom);
              dirCombo->setCurrentIndex(static_cast<int>(Custom));
            }
            // Get camera direction
            auto* rv = renderView();
            if (!rv) {
              return;
            }
            auto* cam = rv->GetActiveCamera();
            double* pos = cam->GetPosition();
            double* fp = cam->GetFocalPoint();
            double n[3] = { fp[0] - pos[0], fp[1] - pos[1], fp[2] - pos[2] };
            setPlaneNormal(n[0], n[1], n[2]);
            // Update the normal text boxes
            for (int i = 0; i < 3; ++i) {
              QSignalBlocker b(normalInputs[i]);
              normalInputs[i]->setText(QString::number(n[i]));
            }
          });

  // Direction combo
  connect(dirCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this, sliceSlider, thickSpin, pointInputs, normalInputs,
           dirCombo](int idx) {
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
              thickSpin->setMaximum(ms > 0 ? ms : 999);
              sliceSlider->setValue(slice() >= 0 ? slice() : 0);
            }
          });

  // Update UI when direction/slice changes from the sink itself, e.g.
  // grabbing the plane's arrow in the view switches it to Custom
  connect(this, &SliceSink::directionChanged, widget,
          [sliceSlider, thickSpin, pointInputs, normalInputs, dirCombo,
           this](Direction dir) {
            int idx = dirCombo->findData(static_cast<int>(dir));
            if (idx >= 0 && idx != dirCombo->currentIndex()) {
              QSignalBlocker blocker(dirCombo);
              dirCombo->setCurrentIndex(idx);
            }
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
              thickSpin->setMaximum(ms > 0 ? ms : 999);
              sliceSlider->setValue(slice() >= 0 ? slice() : 0);
            }
          });
  connect(this, &SliceSink::sliceChanged, widget, [sliceSlider](int s) {
    QSignalBlocker blocker(sliceSlider);
    sliceSlider->setValue(s);
  });

  // Update point/normal text inputs when the plane is dragged in the 3D view
  connect(this, &SliceSink::planeChanged, widget,
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

QJsonObject SliceSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["direction"] = static_cast<int>(m_direction);
  json["slice"] = m_slice;
  json["opacity"] = m_opacity;
  json["sliceThickness"] = m_sliceThickness;
  json["thickSliceMode"] = static_cast<int>(m_thickSliceMode);
  json["interpolate"] = m_interpolate;
  json["showArrow"] = m_showArrow;
  json["mapScalars"] = m_mapScalars;
  json["activeScalars"] = activeScalarsToName(m_activeScalars);
  json["linked"] = m_linked;

  // Serialize plane geometry from widget if available
  double point[3];
  if (m_widget) {
    QJsonArray center, normal;
    m_widget->GetCenter(point);
    center = { point[0], point[1], point[2] };
    m_widget->GetNormal(point);
    normal = { point[0], point[1], point[2] };
    json["planeCenter"] = center;
    json["planeNormal"] = normal;

    // Also save origin/point1/point2 for exact reconstruction
    QJsonArray origin, point1, point2;
    m_widget->GetOrigin(point);
    origin = { point[0], point[1], point[2] };
    m_widget->GetPoint1(point);
    point1 = { point[0], point[1], point[2] };
    m_widget->GetPoint2(point);
    point2 = { point[0], point[1], point[2] };
    json["origin"] = origin;
    json["point1"] = point1;
    json["point2"] = point2;
  }

  return json;
}

bool SliceSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("direction")) {
    m_direction = static_cast<Direction>(json["direction"].toInt());
  }
  if (json.contains("slice")) {
    m_slice = json["slice"].toInt();
  }
  if (json.contains("linked")) {
    m_linked = json["linked"].toBool();
  }
  if (json.contains("opacity")) {
    setOpacity(json["opacity"].toDouble());
  }
  if (json.contains("sliceThickness")) {
    setSliceThickness(json["sliceThickness"].toInt());
  }
  if (json.contains("thickSliceMode")) {
    setThickSliceMode(
      static_cast<ThickSliceMode>(json["thickSliceMode"].toInt()));
  }
  if (json.contains("interpolate")) {
    setTextureInterpolate(json["interpolate"].toBool());
  }
  if (json.contains("showArrow")) {
    setShowArrow(json["showArrow"].toBool());
  }
  if (json.contains("mapScalars")) {
    setMapScalars(json["mapScalars"].toBool());
  }
  if (json.contains("activeScalars")) {
    readActiveScalars(json, m_activeScalars);
  }
  // Restore plane geometry — but only for Custom (non-orthogonal)
  // slices. For orthogonal directions (XY / YZ / XZ), the plane is
  // uniquely determined by direction + slice index + data bounds, and
  // applyDirection() sets it via SetPlaneOrientation + SetSliceIndex.
  // Overwriting origin/point1/point2 after that puts the widget into a
  // hybrid state where the orientation member still says "orthogonal"
  // but the plane coords were imposed externally; subsequent
  // SetSliceIndex(N) from the slider then fails with "Bad plane
  // coordinate system" in vtkPlaneSource.
  if (m_widget && m_direction == Custom && json.contains("origin") &&
      json.contains("point1") && json.contains("point2")) {
    auto o = json["origin"].toArray();
    auto p1 = json["point1"].toArray();
    auto p2 = json["point2"].toArray();
    double origin[3] = { o[0].toDouble(), o[1].toDouble(), o[2].toDouble() };
    double pt1[3] = { p1[0].toDouble(), p1[1].toDouble(), p1[2].toDouble() };
    double pt2[3] = { p2[0].toDouble(), p2[1].toDouble(), p2[2].toDouble() };
    m_widget->SetOrigin(origin);
    m_widget->SetPoint1(pt1);
    m_widget->SetPoint2(pt2);
    m_widget->UpdatePlacement();
  }
  if (json.contains("planeCenter")) {
    auto c = json["planeCenter"].toArray();
    m_planeCenter[0] = c[0].toDouble();
    m_planeCenter[1] = c[1].toDouble();
    m_planeCenter[2] = c[2].toDouble();
    m_planeCenterSet = true;
  }
  if (json.contains("planeNormal")) {
    auto n = json["planeNormal"].toArray();
    m_planeNormal[0] = n[0].toDouble();
    m_planeNormal[1] = n[1].toDouble();
    m_planeNormal[2] = n[2].toDouble();
  }

  // If the sink has already consumed (m_dims populated), the widget's
  // plane was set up by an earlier applyDirection() using the default
  // direction — re-apply now that we know the saved direction + slice
  // so the dropdown, plane geometry, and slice index all match. If
  // consume() hasn't run yet, applyDirection() will be called as part
  // of its normal flow with the saved values in place.
  if (m_widget && m_dims[0] > 0 && m_dims[1] > 0 && m_dims[2] > 0) {
    applyDirection();
  }

  return true;
}

void SliceSink::onMetadataChanged()
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

  applyActiveScalars();
  emit renderNeeded();
}

} // namespace pipeline
} // namespace tomviz
