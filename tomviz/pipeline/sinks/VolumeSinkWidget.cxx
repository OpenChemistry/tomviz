/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumeSinkWidget.h"
#include "ui_VolumeLightingForm.h"
#include "ui_VolumeSinkWidget.h"

#include "vtkVolumeMapper.h"

#include "LightingPresetStore.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace tomviz {

namespace {

/// Enable or disable @a w, showing @a reason as its tool tip while it is
/// disabled and putting the tool tip Designer set back afterwards.
void setEnabledWithReason(QWidget* w, bool enabled, const QString& reason)
{
  static const char* kOriginalToolTip = "tomvizOriginalToolTip";
  w->setEnabled(enabled);
  if (!enabled) {
    if (!w->property(kOriginalToolTip).isValid()) {
      w->setProperty(kOriginalToolTip, w->toolTip());
    }
    w->setToolTip(reason);
  } else if (w->property(kOriginalToolTip).isValid()) {
    w->setToolTip(w->property(kOriginalToolTip).toString());
    w->setProperty(kOriginalToolTip, QVariant());
  }
}

} // namespace

// If we make this bigger, such as 1000, and we make the max too
// close to the data minimum or the min too close to the data maximum,
// we run into errors like these:
// ( 118.718s) [paraview        ]vtkOpenGLVolumeLookupTa:84    WARN|
// vtkOpenGLVolumeRGBTable (0x55ba0c5cc970): This OpenGL implementation does not
// support the required texture size of 65536, falling back to maximum allowed,
// 32768.This may cause an incorrect lookup table mapping.
static const double RANGE_INCREMENT = 500;

VolumeSinkWidget::VolumeSinkWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::VolumeSinkWidget),
    m_uiLighting(new Ui::VolumeLightingForm)
{
  m_ui->setupUi(this);

  QWidget* lightingWidget = new QWidget;
  m_uiLighting->setupUi(lightingWidget);
  QWidget::layout()->addWidget(lightingWidget);
  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  const int leWidth = 50;
  m_uiLighting->sliAmbient->setLineEditWidth(leWidth);
  m_uiLighting->sliDiffuse->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecular->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecularPower->setLineEditWidth(leWidth);
  m_uiLighting->sliShadows->setLineEditWidth(leWidth);
  m_uiLighting->sliShadowReach->setLineEditWidth(leWidth);
  m_uiLighting->sliAnisotropy->setLineEditWidth(leWidth);

  m_uiLighting->sliSpecularPower->setMaximum(150);
  m_uiLighting->sliSpecularPower->setMinimum(1);
  m_uiLighting->sliSpecularPower->setResolution(200);

  m_uiLighting->sliShadows->setMaximum(2.0);
  m_uiLighting->sliShadows->setResolution(200);
  // Commit on release: leaving zero asks for confirmation, and a dialog
  // popping up on the first tick of a drag would cut the drag short.
  m_uiLighting->sliShadows->setSliderTracking(false);
  m_uiLighting->sliAnisotropy->setMinimum(-1.0);
  m_uiLighting->sliAnisotropy->setMaximum(1.0);
  m_uiLighting->sliAnisotropy->setResolution(200);

  // Advanced section starts collapsed; the presets are the primary control.
  m_uiLighting->advancedWidget->setVisible(false);
  connect(m_uiLighting->expAdvanced, &pqExpanderButton::toggled,
          m_uiLighting->advancedWidget, &QWidget::setVisible);

  // Shown only while the view's volumes are rendered together; see
  // setMultiVolumeMode.
  m_multiVolumeNote = new QLabel(lightingWidget);
  m_multiVolumeNote->setWordWrap(true);
  m_multiVolumeNote->setVisible(false);
  m_uiLighting->lightingLayout->insertWidget(0, m_multiVolumeNote);

  const auto presets = presetButtons();
  for (int i = 0; i < presets.size(); ++i) {
    connect(presets[i], &QPushButton::clicked, this,
            [this, i]() { emit lightingPresetClicked(i); });
  }

  // Saved presets: the user's own bundles, below the built-in buttons
  auto* userRow = new QHBoxLayout;
  m_userPresets = new QComboBox(this);
  m_userPresets->setToolTip("Lighting settings you saved earlier.");
  auto* saveUserPreset = new QPushButton("Save...", this);
  saveUserPreset->setToolTip("Save the current lighting settings under a name.");
  m_renameUserPreset = new QPushButton("Rename...", this);
  m_renameUserPreset->setToolTip("Give the selected saved preset a new name.");
  m_deleteUserPreset = new QPushButton("Delete", this);
  m_deleteUserPreset->setToolTip("Remove the selected saved preset.");
  userRow->addWidget(m_userPresets, 1);
  userRow->addWidget(saveUserPreset);
  userRow->addWidget(m_renameUserPreset);
  userRow->addWidget(m_deleteUserPreset);
  m_uiLighting->lightingLayout->insertLayout(1, userRow);
  refreshUserLightingPresets();
  connect(&pipeline::LightingPresetStore::instance(), &pipeline::LightingPresetStore::changed,
          this, &VolumeSinkWidget::refreshUserLightingPresets);
  connect(m_userPresets, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            m_renameUserPreset->setEnabled(idx > 0);
            m_deleteUserPreset->setEnabled(idx > 0);
            if (idx > 0) {
              emit userLightingPresetSelected(m_userPresets->itemText(idx));
            }
          });
  connect(saveUserPreset, &QPushButton::clicked, this,
          &VolumeSinkWidget::saveUserLightingPresetRequested);
  connect(m_renameUserPreset, &QPushButton::clicked, this, [this]() {
    if (m_userPresets->currentIndex() > 0) {
      emit renameUserLightingPresetRequested(m_userPresets->currentText());
    }
  });
  connect(m_deleteUserPreset, &QPushButton::clicked, this, [this]() {
    if (m_userPresets->currentIndex() > 0) {
      emit deleteUserLightingPresetRequested(m_userPresets->currentText());
    }
  });

  m_ui->soliditySlider->setLineEditWidth(leWidth);

  QStringList labelsBlending;
  labelsBlending << tr("Composite") << tr("Max") << tr("Min") << tr("Average")
                 << tr("Additive");
  m_ui->cbBlending->addItems(labelsBlending);

  QStringList labelsTransferMode;
  labelsTransferMode << tr("Scalar") << tr("Scalar-Gradient 1D")
                     << tr("Scalar-Gradient 2D");
  m_ui->cbTransferMode->addItems(labelsTransferMode);

  QStringList labelsInterp;
  labelsInterp << tr("Nearest Neighbor") << tr("Linear");
  m_ui->cbInterpolation->addItems(labelsInterp);

  connect(m_ui->cbJittering, &QCheckBox::toggled, this,
          &VolumeSinkWidget::jitteringToggled);
  connect(m_ui->cbBlending, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VolumeSinkWidget::onBlendingChanged);
  connect(m_ui->cbInterpolation,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VolumeSinkWidget::interpolationChanged);
  connect(m_ui->cbTransferMode,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VolumeSinkWidget::transferModeChanged);

  connect(m_ui->useRgbaMapping, &QCheckBox::toggled, this,
          &VolumeSinkWidget::useRgbaMappingToggled);

  connect(m_ui->rgbaMappingCombineComponents, &QCheckBox::toggled, this,
          &VolumeSinkWidget::rgbaMappingCombineComponentsToggled);
  connect(m_ui->rgbaMappingComponent, &QComboBox::currentTextChanged, this,
          &VolumeSinkWidget::rgbaMappingComponentChanged);

  // Using QueuedConnections here to circumvent DoubleSliderWidget->BlockUpdate
  connect(m_ui->sliRgbaMappingMin, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::onRgbaMappingMinChanged, Qt::QueuedConnection);
  connect(m_ui->sliRgbaMappingMax, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::onRgbaMappingMaxChanged, Qt::QueuedConnection);

  connect(m_uiLighting->cbShading, &QCheckBox::toggled, this,
          &VolumeSinkWidget::lightingToggled);
  connect(m_uiLighting->sliAmbient, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::ambientChanged);
  connect(m_uiLighting->sliDiffuse, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::diffuseChanged);
  connect(m_uiLighting->sliSpecular, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::specularChanged);
  connect(m_uiLighting->sliSpecularPower, &DoubleSliderWidget::valueEdited,
          this, &VolumeSinkWidget::specularPowerChanged);
  connect(m_uiLighting->sliShadows, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::volumetricScatteringChanged);
  connect(m_uiLighting->cbShadows, &QCheckBox::toggled, this,
          &VolumeSinkWidget::shadowsToggled);
  connect(m_uiLighting->sliShadowReach, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::shadowReachChanged);
  connect(m_uiLighting->sliAnisotropy, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::anisotropyChanged);
  connect(m_uiLighting->cbSmoothNormals, &QCheckBox::toggled, this,
          &VolumeSinkWidget::smoothNormalsToggled);
  connect(m_ui->soliditySlider, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::solidityChanged);

  // FIXME: staged for removal
  m_ui->cbTransferMode->setVisible(false);
  m_ui->label->setVisible(false);

  // FIXME: staged for removal
  m_ui->useRgbaMapping->setVisible(false);
  m_ui->groupRgbaMappingRange->setVisible(false);
  m_ui->rgbaMappingComponentLabel->setVisible(false);
  m_ui->rgbaMappingComponent->setVisible(false);
}

VolumeSinkWidget::~VolumeSinkWidget() = default;

void VolumeSinkWidget::setJittering(const bool enable)
{
  m_ui->cbJittering->setChecked(enable);
}

void VolumeSinkWidget::setBlendingMode(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode) &&
                                       !m_lightingShared);
  m_ui->cbBlending->setCurrentIndex(static_cast<int>(mode));
}

void VolumeSinkWidget::setInterpolationType(const int type)
{
  m_ui->cbInterpolation->setCurrentIndex(type);
}

void VolumeSinkWidget::setLighting(const bool enable)
{
  m_uiLighting->cbShading->setChecked(enable);
}

void VolumeSinkWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(value);
}

void VolumeSinkWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(value);
}

void VolumeSinkWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(value);
}

void VolumeSinkWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(value);
}

void VolumeSinkWidget::setVolumetricScattering(const double value)
{
  m_uiLighting->sliShadows->setValue(value);
}

void VolumeSinkWidget::setShadowsEnabled(const bool enable)
{
  m_uiLighting->cbShadows->setChecked(enable);
  updateShadowControlsEnabled();
}

void VolumeSinkWidget::setScatteringOverBudget(const bool overBudget)
{
  m_uiLighting->laOverBudget->setVisible(overBudget);
}

void VolumeSinkWidget::updateShadowControlsEnabled()
{
  // Editing a value that cannot show up in the render is just confusing, so
  // follow the switch - and the switch itself follows availability.
  const bool enable = m_scatteringAvailable &&
                      m_uiLighting->cbShadows->isChecked();
  m_uiLighting->laShadows->setEnabled(enable);
  m_uiLighting->sliShadows->setEnabled(enable);
  m_uiLighting->laShadowReach->setEnabled(enable);
  m_uiLighting->sliShadowReach->setEnabled(enable);
  m_uiLighting->laAnisotropy->setEnabled(enable);
  m_uiLighting->sliAnisotropy->setEnabled(enable);
}

void VolumeSinkWidget::setShadowReach(const double value)
{
  m_uiLighting->sliShadowReach->setValue(value);
}

void VolumeSinkWidget::setAnisotropy(const double value)
{
  m_uiLighting->sliAnisotropy->setValue(value);
}

void VolumeSinkWidget::setSmoothNormals(const bool enable)
{
  m_uiLighting->cbSmoothNormals->setChecked(enable);
}

void VolumeSinkWidget::setActiveLightingPreset(const int preset)
{
  const auto presets = presetButtons();
  for (int i = 0; i < presets.size(); ++i) {
    presets[i]->setChecked(i == preset);
  }
}

void VolumeSinkWidget::refreshUserLightingPresets()
{
  QSignalBlocker blocker(m_userPresets);
  auto current = m_userPresets->currentIndex() > 0
                   ? m_userPresets->currentText()
                   : QString();
  m_userPresets->clear();
  m_userPresets->addItem("Saved presets...");
  for (const auto& preset : pipeline::LightingPresetStore::instance().presets()) {
    m_userPresets->addItem(preset.name);
  }
  int idx = current.isEmpty() ? -1 : m_userPresets->findText(current);
  m_userPresets->setCurrentIndex(idx < 0 ? 0 : idx);
  m_renameUserPreset->setEnabled(m_userPresets->currentIndex() > 0);
  m_deleteUserPreset->setEnabled(m_userPresets->currentIndex() > 0);
}

void VolumeSinkWidget::setActiveUserLightingPreset(const QString& name)
{
  QSignalBlocker blocker(m_userPresets);
  int idx = name.isEmpty() ? -1 : m_userPresets->findText(name);
  m_userPresets->setCurrentIndex(idx < 0 ? 0 : idx);
  m_renameUserPreset->setEnabled(m_userPresets->currentIndex() > 0);
  m_deleteUserPreset->setEnabled(m_userPresets->currentIndex() > 0);
}

QList<QPushButton*> VolumeSinkWidget::presetButtons() const
{
  return { m_uiLighting->btnFlat, m_uiLighting->btnSimple,
           m_uiLighting->btnGentle, m_uiLighting->btnSoft,
           m_uiLighting->btnFull };
}

void VolumeSinkWidget::setScatteringAvailable(const bool available,
                                              const QString& reason)
{
  m_scatteringAvailable = available;
  for (auto* w : scatteringWidgets()) {
    setEnabledWithReason(w, available, reason);
  }
  updateShadowControlsEnabled();
}

QList<QWidget*> VolumeSinkWidget::scatteringWidgets() const
{
  // The presets that cast volumetric shadows, plus the Advanced controls
  // that drive them. Everything else in the panel stays usable.
  return { m_uiLighting->btnSoft, m_uiLighting->btnFull,
           m_uiLighting->cbShadows, m_uiLighting->sliShadows,
           m_uiLighting->sliShadowReach, m_uiLighting->sliAnisotropy };
}

void VolumeSinkWidget::onBlendingChanged(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode) &&
                                       !m_lightingShared);
  emit blendingChanged(mode);
}

bool VolumeSinkWidget::usesLighting(const int mode) const
{
  if (mode == vtkVolumeMapper::COMPOSITE_BLEND) {
    return true;
  }

  return false;
}

void VolumeSinkWidget::setTransferMode(const int transferMode)
{
  m_ui->cbTransferMode->setCurrentIndex(transferMode);
}

void VolumeSinkWidget::setSolidity(const double value)
{
  m_ui->soliditySlider->setValue(value);
}

void VolumeSinkWidget::setRgbaMappingAllowed(const bool b)
{
  m_ui->useRgbaMapping->setVisible(b);

  if (!b) {
    setUseRgbaMapping(false);
  }
}

void VolumeSinkWidget::setUseRgbaMapping(const bool b)
{
  m_ui->useRgbaMapping->setChecked(b);
}

void VolumeSinkWidget::setRgbaMappingMin(const double v)
{
  m_ui->sliRgbaMappingMin->setValue(v);
}

void VolumeSinkWidget::setRgbaMappingMax(const double v)
{
  m_ui->sliRgbaMappingMax->setValue(v);
}

void VolumeSinkWidget::setRgbaMappingSliderRange(const double range[2])
{
  double min = range[0];
  double max = range[1];
  m_ui->sliRgbaMappingMin->setMinimum(min);
  m_ui->sliRgbaMappingMin->setMaximum(max);
  m_ui->sliRgbaMappingMax->setMinimum(min);
  m_ui->sliRgbaMappingMax->setMaximum(max);
}

void VolumeSinkWidget::setRgbaMappingCombineComponents(const bool b)
{
  m_ui->rgbaMappingCombineComponents->setChecked(b);
  m_ui->rgbaMappingComponent->setVisible(!b);
  m_ui->rgbaMappingComponentLabel->setVisible(!b);
}

void VolumeSinkWidget::setRgbaMappingComponentOptions(
  const QStringList& components)
{
  m_ui->rgbaMappingComponent->clear();
  m_ui->rgbaMappingComponent->addItems(components);
}

void VolumeSinkWidget::setRgbaMappingComponent(const QString& component)
{
  m_ui->rgbaMappingComponent->setCurrentText(component);
}

void VolumeSinkWidget::setMultiVolumeMode(const bool active, const bool lead,
                                          const QString& leadLabel)
{
  const QString reason =
    tr("The volumes in this view are rendered together, which always "
       "composites with ray jittering.");
  setEnabledWithReason(m_ui->label_4, !active, reason);
  setEnabledWithReason(m_ui->cbBlending, !active, reason);
  setEnabledWithReason(m_ui->cbJittering, !active, reason);

  m_lightingShared = active && !lead;
  m_uiLighting->gbLighting->setEnabled(
    usesLighting(m_ui->cbBlending->currentIndex()) && !m_lightingShared);
  if (active) {
    m_multiVolumeNote->setText(
      lead ? tr("Applies to every volume rendered together in this view.")
           : tr("Shared by the volumes rendered together in this view and "
                "set on \"%1\".")
               .arg(leadLabel));
  }
  m_multiVolumeNote->setVisible(active);
}

void VolumeSinkWidget::onRgbaMappingMinChanged(double v)
{
  // Compute an increment. Don't let the min value get closer
  // than this to the maximum.
  double fullRange[2] = { m_ui->sliRgbaMappingMax->minimum(),
                          m_ui->sliRgbaMappingMax->maximum() };
  double increment = (fullRange[1] - fullRange[0]) / RANGE_INCREMENT;
  double trueMaximum = fullRange[1] - increment;
  if (v > trueMaximum) {
    setRgbaMappingMin(trueMaximum);
    v = trueMaximum;
  }

  double currentMax = m_ui->sliRgbaMappingMax->value();
  if (v > currentMax) {
    // Set the maximum to be an increment above...
    setRgbaMappingMax(v + increment);
  }

  emit rgbaMappingMinChanged(v);
}

void VolumeSinkWidget::onRgbaMappingMaxChanged(double v)
{
  // Compute an increment. Don't let the max value get closer
  // than this to the minimum.
  double fullRange[2] = { m_ui->sliRgbaMappingMin->minimum(),
                          m_ui->sliRgbaMappingMin->maximum() };
  double increment = (fullRange[1] - fullRange[0]) / RANGE_INCREMENT;
  double trueMinimum = fullRange[0] + increment;
  if (v < trueMinimum) {
    setRgbaMappingMax(trueMinimum);
    v = trueMinimum;
  }

  double currentMin = m_ui->sliRgbaMappingMin->value();
  if (v < currentMin) {
    // Set the minimum to be an increment below...
    setRgbaMappingMin(v - increment);
  }

  emit rgbaMappingMaxChanged(v);
}

QFormLayout* VolumeSinkWidget::formLayout()
{
  return m_ui->formLayout;
}

QVBoxLayout* VolumeSinkWidget::mainLayout()
{
  return qobject_cast<QVBoxLayout*>(QWidget::layout());
}

void VolumeSinkWidget::setCategoricalMode(const bool categorical)
{
  // Blending averages, maximizes or sums the scalars along the ray;
  // done to label numbers that produces a label nothing in the data
  // carries. Composite is the only mode that means anything here.
  m_ui->label_4->setVisible(!categorical);
  m_ui->cbBlending->setVisible(!categorical);
  if (categorical) {
    m_ui->cbBlending->setCurrentIndex(vtkVolumeMapper::COMPOSITE_BLEND);
  }

  // Linear interpolation samples between neighboring voxels, which for
  // labels 2 and 6 yields 4 -- a different label's color, drawn along
  // every boundary. VolumeSink pins the setting to nearest for a label
  // map; hide the control rather than offer a choice that is wrong.
  m_ui->label_5->setVisible(!categorical);
  m_ui->cbInterpolation->setVisible(!categorical);
}
} // namespace tomviz
