/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LabelMapSink.h"

#include "DoubleSliderWidget.h"
#include "LabelMapSurface.h"
#include "LabelTableWidget.h"
#include "VolumeSinkWidget.h"
#include "InputPort.h"
#include "data/LabelMapData.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <vtkActor.h>
#include <vtkImageData.h>
#include <vtkPVRenderView.h>
#include <vtkPlane.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkSmartPointer.h>
#include <vtkVolumeProperty.h>

#include <algorithm>

namespace tomviz {
namespace pipeline {

LabelMapSink::LabelMapSink(QObject* parent) : VolumeSink(parent)
{
  setLabel("Label Map");

  // Any volume is accepted, not just a port already typed as a label
  // map. A segmentation loaded from a file arrives as an ordinary
  // volume, because the reader cannot know the numbers are labels, and
  // refusing it would leave no way to say so. What the menu offers is
  // filtered by canInterpretAsLabelMap() instead; a link made by hand
  // to something continuous is allowed and simply lists no labels.
  inputPorts()[0]->setAcceptedTypes(PortType::ImageData);

  // Switching the detached color map on hands us a brand new transfer
  // function pair, which starts out as ParaView's default ramp. Project
  // the labels onto it so the render does not flash a rainbow.
  connect(this, &LegacyModuleSink::colorMapChanged, this, [this]() {
    applyLabels();
    emit labelsChanged();
  });

  // The volume representation has no shading normal past the one-voxel
  // boundary shell of a label, so keep the ray step fine enough that the
  // first hit usually lands in it.
  setFineSampling(true);

  // The surface representation. Colors are written per face by
  // colorLabelSurface, so the mapper takes them as they are.
  m_surfaceMapper->SetScalarModeToUseCellData();
  m_surfaceMapper->SetColorModeToDirectScalars();
  m_surfaceMapper->ScalarVisibilityOn();
  m_surfaceProperty->SetAmbient(0.1);
  m_surfaceProperty->SetDiffuse(0.9);
  m_surfaceProperty->SetSpecular(0.2);
  m_surfaceProperty->SetSpecularPower(30.0);
  m_surfaceProperty->SetRepresentationToSurface();
  m_surfaceActor->SetMapper(m_surfaceMapper);
  m_surfaceActor->SetProperty(m_surfaceProperty);
  m_surfaceActor->SetVisibility(0);
}

LabelMapSink::~LabelMapSink() = default;

bool LabelMapSink::volumeRenderingEnabled() const
{
  return m_representation == Representation::Volume;
}

LabelMapSink::Representation LabelMapSink::representation() const
{
  return m_representation;
}

void LabelMapSink::setRepresentation(Representation representation)
{
  if (m_representation == representation) {
    return;
  }
  m_representation = representation;
  // Re-applies the sink's own visibility to the volume props, which now
  // read volumeRenderingEnabled() the other way.
  setVisibility(visibility());
  updateSurface();
  emit representationChanged();
  emit renderNeeded();
}

int LabelMapSink::surfaceSmoothing() const
{
  return m_surfaceSmoothing;
}

void LabelMapSink::setSurfaceSmoothing(int iterations)
{
  iterations = std::clamp(iterations, 0, kMaxSurfaceSmoothing);
  if (m_surfaceSmoothing == iterations) {
    return;
  }
  m_surfaceSmoothing = iterations;
  updateSurface();
  emit representationChanged();
}

double LabelMapSink::surfaceOpacity() const
{
  return m_surfaceProperty->GetOpacity();
}

void LabelMapSink::setSurfaceOpacity(double opacity)
{
  m_surfaceProperty->SetOpacity(std::clamp(opacity, 0.0, 1.0));
  emit representationChanged();
  emit renderNeeded();
}

vtkPolyData* LabelMapSink::surface() const
{
  return m_surface;
}

void LabelMapSink::setVisibility(bool visible)
{
  VolumeSink::setVisibility(visible);
  showSurfaceActor();
}

bool LabelMapSink::initialize(vtkSMViewProxy* view)
{
  if (!VolumeSink::initialize(view)) {
    return false;
  }
  renderView()->AddPropToRenderer(m_surfaceActor);
  return true;
}

bool LabelMapSink::finalize()
{
  if (renderView()) {
    renderView()->RemovePropFromRenderer(m_surfaceActor);
  }
  return VolumeSink::finalize();
}

void LabelMapSink::clearVisualization()
{
  VolumeSink::clearVisualization();
  m_surfaceActor->SetVisibility(0);
}

void LabelMapSink::addClippingPlane(vtkPlane* plane)
{
  VolumeSink::addClippingPlane(plane);
  if (plane) {
    m_surfaceMapper->AddClippingPlane(plane);
    emit renderNeeded();
  }
}

void LabelMapSink::removeClippingPlane(vtkPlane* plane)
{
  VolumeSink::removeClippingPlane(plane);
  if (plane) {
    m_surfaceMapper->RemoveClippingPlane(plane);
    emit renderNeeded();
  }
}

void LabelMapSink::onMetadataChanged()
{
  VolumeSink::onMetadataChanged();
  applySurfaceTransform();
}

void LabelMapSink::showSurfaceActor()
{
  bool shown = visibility() && m_representation == Representation::Surface &&
               m_surface && m_surface->GetNumberOfCells() > 0;
  m_surfaceActor->SetVisibility(shown ? 1 : 0);
}

void LabelMapSink::applySurfaceTransform()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid() || !m_surface) {
    return;
  }
  auto orient = vol->displayOrientation();
  m_surfaceActor->SetOrientation(orient.data());

  // The mesh has the origin and spacing that were current when it was
  // extracted baked into its points. Follow later metadata edits with
  // the actor's position and scale rather than re-extracting.
  auto origin = vol->origin();
  auto spacing = vol->spacing();
  auto displayPos = vol->displayPosition();
  double pos[3];
  double scale[3];
  for (int i = 0; i < 3; ++i) {
    pos[i] = displayPos[i] + origin[i] - m_surfaceOrigin[i];
    scale[i] = m_surfaceSpacing[i] != 0.0 ? spacing[i] / m_surfaceSpacing[i]
                                          : 1.0;
  }
  m_surfaceActor->SetPosition(pos);
  m_surfaceActor->SetScale(scale);
}

void LabelMapSink::updateSurface()
{
  auto labels = labelMap();
  auto vol = volumeData();
  if (m_representation != Representation::Surface || !labels || !vol ||
      !vol->isValid() || !labels->labelsSupported()) {
    m_surfaceActor->SetVisibility(0);
    return;
  }

  // 0 is the conventional background label; it is never a region.
  const double background = 0.0;
  MeshKey key;
  key.image = vol->imageData();
  key.imageTime = key.image->GetMTime();
  key.regions = regionLabels(labels->labels(), background);
  key.smoothing = m_surfaceSmoothing;
  const auto visible = visibleLabels(labels->labels(), background);

  if (!m_mesh || !(key == m_meshKey)) {
    m_mesh = extractLabelMesh(key.image, key.regions, key.smoothing,
                              background);
    m_meshKey = key;
    m_surfaceOrigin = vol->origin();
    m_surfaceSpacing = vol->spacing();
    m_surface = nullptr;
  }
  // Showing or hiding a label only re-selects faces from the mesh, so a
  // checkbox never costs another pass over the volume.
  if (!m_surface || visible != m_surfaceVisible) {
    m_surface = selectLabelFaces(m_mesh, visible);
    m_surfaceVisible = visible;
    m_surfaceMapper->SetInputData(m_surface);
  }
  // Cheap, so a color edit never costs an extraction.
  colorLabelSurface(m_surface, labels->labels(), background);

  applySurfaceTransform();
  showSurfaceActor();
  emit renderNeeded();
}

QIcon LabelMapSink::icon() const
{
  return QIcon(QStringLiteral(":/pipeline/port_labelmap.svg"));
}

LabelMapDataPtr LabelMapSink::labelMap() const
{
  if (auto labels = labelMapData(volumeData())) {
    return labels;
  }
  return m_adopted;
}

bool LabelMapSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!VolumeSink::consume(inputs)) {
    return false;
  }

  auto volume = volumeData();
  if (labelMapData(volume)) {
    // The port carries a real label map, so its own table is the one to
    // use and any view built for an earlier input is stale.
    m_adopted.reset();
  } else if (canInterpretAsLabelMap(volume)) {
    // Read a plain volume as labels. The view wraps the same voxels, so
    // nothing is copied, but the table and the transfer functions it
    // drives belong to this sink alone.
    if (!m_adopted || m_adopted->imageData() != volume->imageData()) {
      m_adopted = std::make_shared<LabelMapData>(
        vtkSmartPointer<vtkImageData>(volume->imageData()));
      // A table restored from a state file describes voxels this sink
      // had not seen yet. Now that it has them, hand it over; the
      // refresh below reconciles it against what is actually there.
      if (!m_restoredAdopted.isEmpty()) {
        m_adopted->labels().deserialize(m_restoredAdopted);
        m_restoredAdopted = QJsonObject();
      }
    }
    // Label bands would otherwise be written into the color map the
    // port's plain volume shares with every other sink reading it,
    // turning an ordinary Volume module next door into a segmentation.
    if (!useDetachedColorMap()) {
      setUseDetachedColorMap(true);
    }
  } else {
    m_adopted.reset();
  }

  // Linear interpolation between labels 2 and 6 samples 4, colored as
  // whichever label owns that value. The control is hidden in this
  // sink's panel, so pin the setting rather than trust what a state
  // file or a base-class default left behind.
  setInterpolationType(VTK_NEAREST_INTERPOLATION);

  // Samples that miss the boundary shell are lit by ambient alone, so a
  // floor keeps them a dimmer shade of their label rather than black.
  // Once, so a user's own lighting survives later executions.
  if (!m_volumeLookApplied) {
    if (ambient() < 0.3) {
      setAmbient(0.3);
    }
    m_volumeLookApplied = true;
  }

  // The producing node normally refreshes the table before publishing
  // (see inheritOutputMetadata), but a payload can reach us without
  // having gone through that - a source node's own output, or a state
  // file's restored data. The rescan is cached on the image's
  // modification time, so repeating it here costs nothing.
  if (auto labels = labelMap()) {
    labels->refreshLabels();
    applyLabels();
  }
  updateSurface();

  emit labelsChanged();
  return true;
}

QJsonObject LabelMapSink::serialize() const
{
  auto json = VolumeSink::serialize();
  json["representation"] =
    m_representation == Representation::Surface ? "Surface" : "Volume";
  json["surfaceSmoothing"] = m_surfaceSmoothing;
  json["surfaceOpacity"] = surfaceOpacity();
  json["volumeLookApplied"] = m_volumeLookApplied;
  // A real label map carries its table in its own payload. An adopted
  // one has nowhere else to put it: the port's payload is a plain
  // volume shared with other sinks, so the colors and names the user
  // gave these labels live here or nowhere.
  if (m_adopted) {
    json["adoptedLabelMap"] = m_adopted->labels().serialize();
  } else if (!m_restoredAdopted.isEmpty()) {
    // Restored but never consumed, so it is still owed a home.
    json["adoptedLabelMap"] = m_restoredAdopted;
  }
  return json;
}

bool LabelMapSink::deserialize(const QJsonObject& json)
{
  if (!VolumeSink::deserialize(json)) {
    return false;
  }
  m_restoredAdopted = json.value("adoptedLabelMap").toObject();
  // A state file written before the surface representation existed
  // was showing a volume; keep that look rather than the new default.
  setRepresentation(json.value("representation").toString() == "Surface"
                      ? Representation::Surface
                      : Representation::Volume);
  if (json.contains("surfaceSmoothing")) {
    setSurfaceSmoothing(json.value("surfaceSmoothing").toInt());
  }
  if (json.contains("surfaceOpacity")) {
    setSurfaceOpacity(json.value("surfaceOpacity").toDouble(1.0));
  }
  // Older files carry lighting the user saw and may have tuned
  m_volumeLookApplied = json.value("volumeLookApplied").toBool(true);
  return true;
}

void LabelMapSink::applyLabels()
{
  updateSurface();

  auto labels = labelMap();
  if (!labels || labels->labels().isEmpty()) {
    return;
  }

  if (useDetachedColorMap()) {
    LabelMapData::applyLabelsToProxy(labels->labels(), colorMap());
  } else {
    labels->applyLabels();
  }

  updateColorMap();
  emit renderNeeded();
}

void LabelMapSink::labelTableEdited()
{
  applyLabels();
  emit labelVisibilityChanged();
}

QVector<double> LabelMapSink::hiddenLabels() const
{
  QVector<double> hidden;
  auto labels = labelMap();
  if (!labels) {
    return hidden;
  }
  for (const auto& entry : labels->labels().entries()) {
    if (!entry.visible && entry.value != 0.0) {
      hidden.append(entry.value);
    }
  }
  std::sort(hidden.begin(), hidden.end());
  return hidden;
}

void LabelMapSink::setHiddenLabels(const QVector<double>& hidden)
{
  auto labels = labelMap();
  if (!labels) {
    return;
  }
  auto& table = labels->labels();
  bool changed = false;
  for (int i = 0; i < table.count(); ++i) {
    const auto& entry = table.at(i);
    if (entry.value == 0.0) {
      continue;
    }
    const bool visible = !hidden.contains(entry.value);
    if (entry.visible != visible) {
      table.setVisible(i, visible);
      changed = true;
    }
  }
  if (!changed) {
    return;
  }
  applyLabels();
  emit labelVisibilityChanged();
}

QWidget* LabelMapSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = VolumeSink::createSinkPropertiesWidget(parent);
  auto* volumeWidget = qobject_cast<VolumeSinkWidget*>(widget);
  if (!volumeWidget) {
    return widget;
  }

  volumeWidget->setCategoricalMode(true);
  auto* form = volumeWidget->formLayout();
  auto* mainLayout = volumeWidget->mainLayout();

  // Representation, ahead of Active Scalars
  auto* repCombo = new QComboBox(volumeWidget);
  repCombo->addItem("Surface", static_cast<int>(Representation::Surface));
  repCombo->addItem("Volume", static_cast<int>(Representation::Volume));
  form->insertRow(0, "Representation", repCombo);
  connect(repCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this, repCombo](int idx) {
            setRepresentation(static_cast<Representation>(
              repCombo->itemData(idx).toInt()));
          });

  // Directly under the form holding Active Scalars, which is what
  // decides where the labels are read from in the first place.
  auto* table = new LabelTableWidget(this, volumeWidget);
  mainLayout->insertWidget(1, table);

  auto* surfaceBox = new QGroupBox("Surface", volumeWidget);
  auto* surfaceForm = new QFormLayout(surfaceBox);
  auto* smoothingSpin = new QSpinBox(surfaceBox);
  smoothingSpin->setRange(0, kMaxSurfaceSmoothing);
  smoothingSpin->setToolTip(
    "Smoothing iterations; 0 shows the raw voxel faces. The smoothing "
    "is shrink-free and never moves the surface more than half a voxel, "
    "so regions keep their size at any setting.");
  // Only commit a typed value once editing is done, since each change
  // re-extracts the surface.
  smoothingSpin->setKeyboardTracking(false);
  surfaceForm->addRow("Smoothing", smoothingSpin);
  auto* opacitySlider = new DoubleSliderWidget(true, surfaceBox);
  opacitySlider->setLineEditWidth(50);
  opacitySlider->setMinimum(0.0);
  opacitySlider->setMaximum(1.0);
  opacitySlider->setResolution(100);
  surfaceForm->addRow("Opacity", opacitySlider);
  mainLayout->insertWidget(2, surfaceBox);
  connect(smoothingSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int value) { setSurfaceSmoothing(value); });
  connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
          [this](double value) { setSurfaceOpacity(value); });

  // Everything else the volume panel offers is about volume rendering.
  // Note what is showing now, so the rows the categorical mode already
  // hid stay hidden when the volume controls come back.
  QList<QWidget*> volumeOnly;
  for (int i = 0; i < mainLayout->count(); ++i) {
    auto* item = mainLayout->itemAt(i);
    auto* w = item ? item->widget() : nullptr;
    if (w && w != table && w != surfaceBox && !w->isHidden()) {
      volumeOnly.append(w);
    }
  }
  // Rows 0 and 1 are Representation and Active Scalars
  QList<int> volumeOnlyRows;
  for (int row = 2; row < form->rowCount(); ++row) {
    bool shown = false;
    for (auto role : { QFormLayout::LabelRole, QFormLayout::FieldRole,
                       QFormLayout::SpanningRole }) {
      auto* item = form->itemAt(row, role);
      if (!item) {
        continue;
      }
      if (auto* w = item->widget()) {
        shown = shown || !w->isHidden();
      } else if (item->layout()) {
        shown = true;
      }
    }
    if (shown) {
      volumeOnlyRows.append(row);
    }
  }

  auto sync = [this, repCombo, smoothingSpin, opacitySlider, surfaceBox,
               volumeOnly, volumeOnlyRows, form]() {
    const bool surface = m_representation == Representation::Surface;
    {
      QSignalBlocker blocker(repCombo);
      repCombo->setCurrentIndex(
        repCombo->findData(static_cast<int>(m_representation)));
    }
    {
      QSignalBlocker blocker(smoothingSpin);
      smoothingSpin->setValue(m_surfaceSmoothing);
    }
    {
      QSignalBlocker blocker(opacitySlider);
      opacitySlider->setValue(surfaceOpacity());
    }
    surfaceBox->setVisible(surface);
    for (auto* w : volumeOnly) {
      w->setVisible(!surface);
    }
    for (int row : volumeOnlyRows) {
      form->setRowVisible(row, !surface);
    }
  };
  sync();
  connect(this, &LabelMapSink::representationChanged, widget, sync);

  return widget;
}

} // namespace pipeline
} // namespace tomviz
