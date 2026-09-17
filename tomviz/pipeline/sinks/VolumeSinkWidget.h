/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizVolumeSinkWidget_h
#define tomvizVolumeSinkWidget_h

#include <QScopedPointer>
#include <QWidget>

class QFormLayout;
class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

/**
 * \brief UI layer of VolumeSink.
 *
 * Signals are forwarded to the actuators on the mapper in VolumeSink.
 * This class is intended to contain only logic related to UI actions.
 */

namespace Ui {
class VolumeSinkWidget;
class VolumeLightingForm;
} // namespace Ui

namespace tomviz {

class VolumeSinkWidget : public QWidget
{
  Q_OBJECT

public:
  VolumeSinkWidget(QWidget* parent_ = nullptr);
  ~VolumeSinkWidget() override;

  //@{
  /**
   * UI update methods. The actual model state is stored in VolumeSink (either
   * in the mapper or serialized), so the UI needs to be updated if the state
   * changes or when constructing the UI.
   */
  void setActiveScalars(const QString& scalars);
  void setJittering(const bool enable);
  void setBlendingMode(const int mode);
  void setInterpolationType(const int type);
  void setLighting(const bool enable);
  void setAmbient(const double value);
  void setDiffuse(const double value);
  void setSpecular(const double value);
  void setSpecularPower(const double value);
  void setVolumetricScattering(const double value);
  void setShadowsEnabled(const bool enable);
  /// Show or hide the explanation for the frame guard having switched
  /// volumetric shadows off.
  void setScatteringOverBudget(const bool overBudget);
  void setShadowReach(const double value);
  void setAnisotropy(const double value);
  void setSmoothNormals(const bool enable);
  /// Highlight the preset button matching the sink's current lighting
  /// state (VolumeSink::LightingPreset); -1 (Custom) unchecks them all.
  void setActiveLightingPreset(const int preset);
  /// Select @a name in the saved-presets combo (empty for none).
  void setActiveUserLightingPreset(const QString& name);
  /// Enable or disable the presets and controls that cast volumetric
  /// shadows. When disabling, @a reason is shown as their tool tip.
  void setScatteringAvailable(const bool available, const QString& reason);
  void setTransferMode(const int transferMode);
  void setSolidity(const double value);
  void setRgbaMappingAllowed(const bool allowed);
  void setUseRgbaMapping(const bool b);
  void setRgbaMappingMin(const double value);
  void setRgbaMappingMax(const double value);
  void setRgbaMappingSliderRange(const double range[2]);
  void setRgbaMappingCombineComponents(const bool b);
  void setRgbaMappingComponentOptions(const QStringList& list);
  void setRgbaMappingComponent(const QString& component);
  /// Reflect the sink being drawn as part of its view's multi-volume.
  /// That path always composites with ray jittering, so those controls
  /// are greyed out while @a active; and it takes its lighting from one
  /// member, so the lighting group is editable only for the @a lead, with
  /// a note naming @a leadLabel on the others.
  void setMultiVolumeMode(const bool active, const bool lead,
                          const QString& leadLabel);
  QFormLayout* formLayout();
  /// The top-level vertical layout, so a sink can slot controls of its
  /// own in among the volume ones.
  QVBoxLayout* mainLayout();
  /// Hide the controls that only make sense for a continuous scalar
  /// field, for a sink rendering categorical data (a label map):
  /// blending, which averages or maximizes label numbers, and
  /// interpolation, whose linear setting invents values between labels.
  void setCategoricalMode(const bool categorical);
  //@}

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void jitteringToggled(const bool state);
  void blendingChanged(const int state);
  void interpolationChanged(const int state);
  void lightingToggled(const bool state);
  void ambientChanged(const double value);
  void diffuseChanged(const double value);
  void specularChanged(const double value);
  void specularPowerChanged(const double value);
  void volumetricScatteringChanged(const double value);
  void shadowsToggled(const bool state);
  void shadowReachChanged(const double value);
  void anisotropyChanged(const double value);
  void smoothNormalsToggled(const bool state);
  void lightingPresetClicked(const int preset);
  void userLightingPresetSelected(const QString& name);
  void saveUserLightingPresetRequested();
  void renameUserLightingPresetRequested(const QString& name);
  void deleteUserLightingPresetRequested(const QString& name);
  void transferModeChanged(const int mode);
  void solidityChanged(const double value);
  void useRgbaMappingToggled(const bool b);
  void rgbaMappingCombineComponentsToggled(const bool b);
  void rgbaMappingMinChanged(const double value);
  void rgbaMappingMaxChanged(const double value);
  void rgbaMappingComponentChanged(const QString& component);
  //@}

private:
  VolumeSinkWidget(const VolumeSinkWidget&) = delete;
  void operator=(const VolumeSinkWidget&) = delete;

  bool usesLighting(const int mode) const;
  /// The preset buttons, indexed by VolumeSink::LightingPreset.
  QList<QPushButton*> presetButtons() const;
  /// Rebuild the saved-presets combo from the store.
  void refreshUserLightingPresets();
  /// Grey out the Advanced shadow controls whenever they cannot do
  /// anything: shadows switched off, or scattering unavailable entirely.
  void updateShadowControlsEnabled();
  /// Controls that only do anything when volumetric scattering is available.
  QList<QWidget*> scatteringWidgets() const;

  // Mirrors the last setScatteringAvailable() call. Read back from the
  // check box instead and a disabled parent (non-composite blending
  // greys the whole group) would latch the shadow controls off.
  bool m_scatteringAvailable = true;
  // The sink is in its view's multi-volume without being its lead, so its
  // lighting is not what renders. Kept here so a blending change does not
  // re-enable the group.
  bool m_lightingShared = false;
  // Explains, at the top of the lighting group, whose lighting applies
  // while the view's volumes are rendered together.
  QLabel* m_multiVolumeNote = nullptr;

  QComboBox* m_userPresets = nullptr;

  QPushButton* m_renameUserPreset = nullptr;
  QPushButton* m_deleteUserPreset = nullptr;

  QScopedPointer<Ui::VolumeSinkWidget> m_ui;
  QScopedPointer<Ui::VolumeLightingForm> m_uiLighting;

private slots:
  void onBlendingChanged(const int mode);
  void onRgbaMappingMinChanged(double value);
  void onRgbaMappingMaxChanged(double value);
};
} // namespace tomviz
#endif
