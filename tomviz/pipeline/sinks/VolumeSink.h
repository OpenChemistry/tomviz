/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineVolumeSink_h
#define tomvizPipelineVolumeSink_h

#include "LegacyModuleSink.h"
#include "LightingPresetStore.h"

#include <QPointer>
#include <QTimer>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class vtkNonOrthoImagePlaneWidget;
class vtkPlane;

class QComboBox;

class vtkCallbackCommand;
class vtkColorTransferFunction;
class vtkImageData;
class vtkMultiBlockDataSet;
class vtkMultiBlockVolumeMapper;
class vtkPiecewiseFunction;
class vtkPlane;
class vtkPlaneCollection;
class vtkVolume;
class vtkVolumeProperty;

namespace tomviz {
namespace pipeline {

class SmartVolumeMapper;

/// Volume rendering visualization sink.
/// Matches the old ModuleVolume VTK pipeline: SmartVolumeMapper + Volume +
/// VolumeProperty with jittering, lighting, blending, interpolation, gradient
/// opacity, clipping planes, and external transfer function support.
class VolumeSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  VolumeSink(QObject* parent = nullptr);
  ~VolumeSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;
  bool isColorMapNeeded() const override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  /// Lighting toggle (shade on/off).
  bool lighting() const;
  void setLighting(bool enabled);

  /// Phong lighting parameters.
  double ambient() const;
  void setAmbient(double value);
  double diffuse() const;
  void setDiffuse(double value);
  double specular() const;
  void setSpecular(double value);
  double specularPower() const;
  void setSpecularPower(double value);

  /// Volumetric scattering blending (0 = surface shading only,
  /// 2 = fully volumetric with shadows). GPU ray-cast mapper parameter.
  /// This is the requested level; nothing is rendered while shadows are
  /// switched off (see setShadowsEnabled).
  double volumetricScattering() const;
  void setVolumetricScattering(double value);

  /// Master switch for volumetric shadows, independent of the preset. When
  /// off the volume renders with plain surface shading, but the scattering
  /// level above is remembered, so switching back on restores the look
  /// without the preset selection ever changing.
  bool shadowsEnabled() const;
  void setShadowsEnabled(bool enabled);

  /// True while the frame guard has given up on scattering for the current
  /// configuration because even its coarsest frame ran over budget. The
  /// volume then renders shaded but shadowless until something changes.
  bool scatteringOverBudget() const;

  /// Shadow reach (GlobalIlluminationReach): 0 = local shadows only,
  /// 1 = shadows across the whole volume.
  double shadowReach() const;
  void setShadowReach(double value);

  /// Scattering anisotropy: 0 = isotropic, +1 = forward, -1 = backward.
  double scatteringAnisotropy() const;
  void setScatteringAnisotropy(double value);

  /// Compute shading normals from the opacity transfer function instead of
  /// the raw scalar gradient (reduces noise-induced shading artifacts).
  bool smoothNormals() const;
  void setSmoothNormals(bool enabled);

  /// Render with @a opacity instead of the scalar opacity the histogram
  /// editor owns. An animation sets this so playback does not overwrite the
  /// curve the user authored; passing nullptr hands rendering back to it.
  void setAnimatedScalarOpacity(vtkPiecewiseFunction* opacity);

  /// Roughly what @a curve costs to render, as the share of its domain it
  /// leaves non-transparent. Only meaningful against another curve on the
  /// same data, which is enough to pick the most expensive of several.
  static double opacityRenderCost(vtkPiecewiseFunction* curve);

  /// Size every frame for @a curve rather than for the curve on screen, so
  /// an exported sequence holds one sharpness throughout instead of
  /// tracking the cost up and down. Null goes back to tracking.
  void setWorstCaseOpacity(vtkPiecewiseFunction* curve);

  /// Named bundles of the lighting parameters above. The int values match
  /// the preset button indices in VolumeSinkWidget.
  enum class LightingPreset
  {
    Custom = -1,
    Flat = 0,
    Simple,
    Gentle,
    Soft,
    Full
  };
  void applyLightingPreset(LightingPreset preset);
  /// The lighting settings as they stand, ready to be saved by name.
  UserLightingPreset currentLightingValues() const;
  void applyUserLightingPreset(const UserLightingPreset& preset);
  /// Name of the saved preset the current settings match, if any.
  QString matchingUserLightingPreset() const;
  /// The preset matching the current parameter values, or Custom.
  LightingPreset currentLightingPreset() const;

  /// Whether volumetric scattering can be bounded well enough to be offered
  /// here. False for bricked volumes (their per-brick mappers are out of
  /// reach) and when the Volume.AllowVolumetricScattering setting is off.
  bool scatteringSupported() const;

  /// Blending mode (vtkVolumeMapper enum).
  int blendingMode() const;
  void setBlendingMode(int mode);

  /// Interpolation type (VTK_NEAREST/LINEAR_INTERPOLATION).
  int interpolationType() const;
  void setInterpolationType(int type);

  /// Ray jittering for noise reduction.
  bool jittering() const;
  void setJittering(bool enabled);

  /// Solidity (1 / ScalarOpacityUnitDistance).
  double solidity() const;
  void setSolidity(double value);

  /// Active scalar array index (-1 = use default active scalars).
  int activeScalars() const;
  void setActiveScalars(int index);

  /// Clipping plane support.
  void addClippingPlane(vtkPlane* plane) override;
  void removeClippingPlane(vtkPlane* plane) override;
  void removeAllClippingPlanes();

  /// Cut-out rendering: remove one octant so the interior is visible
  /// from outside. The corner sits at the cutOutPosition fractions;
  /// cutOutCorner picks the removed octant (bit 0 = high X, bit 1 =
  /// high Y, bit 2 = high Z).
  bool cutOutEnabled() const;
  void setCutOutEnabled(bool enabled);
  int cutOutCorner() const;
  void setCutOutCorner(int corner);
  /// Position of the cut along @a axis, as a fraction (0-1) of the
  /// volume's extent.
  double cutOutPosition(int axis) const;
  void setCutOutPosition(int axis, double fraction);

  /// Exploded view: render the volume as evenly sized slabs along one
  /// axis, pulled apart by a gap, without touching the data. Mutually
  /// exclusive with the cut-out, and unsupported on bricked volumes.
  bool explodedEnabled() const;
  /// Animations pass @a refitCamera false: they run while a camera path
  /// may own the camera, and a refit mid-flight would yank it.
  void setExplodedEnabled(bool enabled, bool refitCamera = true);
  /// 0-2 for X, Y, Z; kExplodedCustomAxis (3) to cut along
  /// explodedDirection() instead.
  int explodedAxis() const;
  void setExplodedAxis(int axis);
  /// The custom direction, in the data's coordinates. Stored as given;
  /// the slabs use it normalized.
  std::array<double, 3> explodedDirection() const;
  void setExplodedDirection(double x, double y, double z);
  /// Whether the draggable arrow that edits the custom direction is
  /// drawn. It only appears while the axis is custom.
  bool explodedShowArrow() const;
  void setExplodedShowArrow(bool show);
  int explodedChunks() const;
  void setExplodedChunks(int chunks);
  /// Gap between slabs as a fraction (0-1) of the axis length.
  double explodedGap() const;
  void setExplodedGap(double fraction);
  /// Shift of every cut plane along the direction, in voxels, so the
  /// gaps can be placed where they are wanted; the volume's own two
  /// faces stay put. Signed. Clamped when used so no slab is thinner
  /// than a voxel; explodedOffsetLimit() is that bound for the current
  /// data and slab count (0 without data).
  int explodedOffset() const;
  void setExplodedOffset(int voxels);
  int explodedOffsetLimit() const;

  /// Volumes sharing a view are rendered together from the second one on
  /// (see MultiVolumeCoordinator). While that is on for this sink, its own
  /// props are out of the renderer and its blending, jittering, cut-out,
  /// exploded view and shadows have no say; lighting follows the lead
  /// volume's.
  bool multiVolumeActive() const;
  /// True when this sink's lighting is the one applied to the set.
  bool multiVolumeLead() const;
  /// Called by the coordinator whenever this sink's place in it changes:
  /// swaps the sink's own props for the shared multi-volume or back, and
  /// refreshes the panel.
  void applyMultiVolumeState();

  ///@{
  /// What the coordinator draws on this sink's behalf.
  vtkVolume* volumeProp() const;
  /// The image the sink's own mapper renders, and the array it selected.
  vtkImageData* renderedImage() const;
  QString renderedArrayName() const;
  vtkPlaneCollection* clippingPlanes() const;
  /// True for one of the planes that bound a slab of a custom-direction
  /// exploded view. They sit among clippingPlanes() but belong to this
  /// sink's own slabs, not to the volume as the coordinator draws it.
  bool isExplodedSlabPlane(vtkPlane* plane) const;
  ///@}

  void onMetadataChanged() override;

signals:
  void interpolationTypeChanged(int type);
  void cutOutChanged();
  void explodedChanged();
  /// Emitted when the sink starts or stops being rendered as part of the
  /// view's multi-volume, or when it becomes or stops being its lead.
  void multiVolumeStateChanged();
  void lightingChanged(bool enabled);
  /// Emitted whenever any lighting parameter changes; the properties widget
  /// uses this to refresh its sliders and the active preset highlight.
  void lightingStateChanged();

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;
  void updateColorMap() override;

  /// Whether the volume props should be drawn at all while this sink is
  /// visible. A subclass that draws the data some other way (LabelMapSink's
  /// surface representation) returns false to keep the volume off screen
  /// without touching the sink's own visibility flag.
  virtual bool volumeRenderingEnabled() const { return true; }

  /// Step rays at half a voxel instead of VTK's heuristic (a voxel or
  /// more). Costs render time; meant for label maps, whose shading
  /// normal exists only in a one-voxel shell that coarser steps skip.
  void setFineSampling(bool enabled);

public:
  bool fineSampling() const;

protected:

private:
  void applyActiveScalars();
  void populateScalarsCombo();

  // Choose between the single-texture SmartVolumeMapper and the bricked
  // vtkMultiBlockVolumeMapper based on whether image exceeds the GPU's
  // GL_MAX_3D_TEXTURE_SIZE, and point m_volume at the right one.
  void updateMapperForInput(vtkImageData* image);
  // GPU's max 3-D texture size (queried from the render window when one is
  // available; a conservative fallback otherwise).
  int maxTextureSize() const;
  // Emit a warning that clipping has no effect on a bricked (over-cap) volume.
  void warnClippingUnsupported() const;
  /// Push the cut-out state onto the mapper for the current bounds.
  void applyCutOut();
  /// Build or tear down the extra slab actors and crop every slab to its
  /// share of the axis.
  void applyExploded();
  void teardownExplodedSlabs();
  /// The unit direction the slabs are cut along, in data coordinates.
  std::array<double, 3> explodedUnitDirection() const;
  /// Keep the two clipping planes that bound each slab of a custom
  /// direction on that slab's mapper, and off it for an axis direction,
  /// where the mapper's own cropping does the cutting.
  void syncExplodedSlabPlanes(bool custom);
  /// Create the direction arrow the first time it is needed and show or
  /// hide it as the axis, the switch and the sink's visibility require.
  void updateExplodedWidget();
  void onExplodedWidgetInteraction();
  void onExplodedWidgetInteractionEnded();
  /// Switch the exploded view on or off; the camera is only refit for a
  /// user request, not when the cut-out displaces it.
  void setExplodedEnabledInternal(bool enabled, bool refitCamera);
  /// Place every slab: the display transform from the metadata, plus each
  /// slab's offset along the exploded axis.
  void applyDisplayTransform();
  /// Volumes composite in prop order, so before each render put the
  /// slabs back to front along the view direction.
  void sortExplodedProps();
  /// Slab 0 plus the extra slabs, for settings that apply to every mapper.
  std::vector<SmartVolumeMapper*> allMappers();
  std::vector<vtkVolume*> allVolumes();
  /// Refit the camera once the rendered extent changed.
  void resetCameraQueued();
  // Log why scatteringSupported() is false.
  void warnScatteringUnsupported() const;
  // Whether the panel should offer scattering right now: supported by the
  // sink's own mapper, and that mapper is the one drawing. The requested
  // level is kept either way, so the look comes back when it applies again.
  bool scatteringAvailable() const;
  // User-facing version of the above, for the properties widget. Empty when
  // scattering is available.
  QString scatteringUnavailableReason() const;
  // Whether this sink has a volume on screen that the view's coordinator
  // could take over: shown, drawn as a volume, and small enough for one
  // texture (a bricked volume cannot join a vtkMultiVolume).
  bool multiVolumeEligible() const;
  // Join or leave the view's coordinator to match multiVolumeEligible().
  void syncMultiVolumeMembership();
  // Put the sink's own props into the renderer, or take them out while
  // the coordinator draws the sink.
  void showStandaloneProps(bool shown);
  // Ask before anything starts casting volumetric shadows, unless the user
  // has opted out. Returns false if they declined.
  bool confirmVolumetricShadows(QWidget* parent) const;
  // The scattering level actually handed to the mapper: the requested one,
  // or zero while shadows are switched off.
  double effectiveScattering() const;
  // Push effectiveScattering() to the mapper.
  void applyScattering();
  // Called at the end of every render of this sink's view. Arms the settle
  // timer after unlit interactive frames (so the scattering look is always
  // restored, even when no end-of-interaction still render arrives) and
  // re-renders when the guard's measurement says a sharper frame now fits
  // the time budget.
  void onRenderFinished();

  vtkNew<SmartVolumeMapper> m_volumeMapper;
  // Used only when a volume exceeds the texture-size cap; renders the volume
  // as resident per-brick textures (see VolumeBricking.h).
  vtkNew<vtkMultiBlockVolumeMapper> m_multiBlockMapper;
  vtkSmartPointer<vtkMultiBlockDataSet> m_brickedVolume;
  bool m_usingMultiBlock = false;

  // Cut-out state; applied to the mapper by applyCutOut().
  bool m_cutOutEnabled = false;
  int m_cutOutCorner = 0;
  double m_cutOutPosition[3] = { 0.5, 0.5, 0.5 };
  // Exploded-view state; applied by applyExploded(). Slab 0 is m_volume
  // and m_volumeMapper; these hold slabs 1..chunks-1.
  bool m_explodedEnabled = false;
  int m_explodedAxis = 2;
  std::array<double, 3> m_explodedDirection = { 1.0, 1.0, 1.0 };
  bool m_explodedShowArrow = true;
  int m_explodedChunks = 4;
  double m_explodedGap = 0.25;
  int m_explodedOffset = 0;
  std::vector<vtkSmartPointer<vtkVolume>> m_explodedVolumes;
  std::vector<vtkSmartPointer<SmartVolumeMapper>> m_explodedMappers;
  // One (lower, upper) pair per slab, slab 0 first, used only while the
  // direction is custom; applyDisplayTransform places them.
  std::vector<std::pair<vtkSmartPointer<vtkPlane>, vtkSmartPointer<vtkPlane>>>
    m_explodedSlabPlanes;
  vtkSmartPointer<vtkNonOrthoImagePlaneWidget> m_explodedWidget;
  // The image the arrow widget was last given, so a re-execution that
  // swaps the image re-feeds it and anything else does not.
  vtkImageData* m_explodedWidgetImage = nullptr;
  unsigned long m_explodedWidgetTag = 0;
  unsigned long m_explodedWidgetEndTag = 0;
  bool m_explodedWidgetDragging = false;
  bool m_explodedOrderReversed = false;
  // Slabs were added or re-added in natural order; re-sort regardless
  bool m_explodedOrderDirty = true;
  vtkNew<vtkCallbackCommand> m_sortObserver;
  unsigned long m_sortObserverId = 0;
  vtkNew<vtkVolume> m_volume;
  vtkNew<vtkVolumeProperty> m_volumeProperty;
  vtkSmartPointer<vtkPiecewiseFunction> m_animatedScalarOpacity;

  // Watches render completion for onRenderFinished().
  vtkNew<vtkCallbackCommand> m_refinementObserver;
  unsigned long m_refinementObserverId = 0;
  // Requests the still render that restores the scattering look once
  // interactive frames stop arriving; armed in onRenderFinished().
  QTimer m_settleTimer;

  // Requested scattering level and the master shadow switch that gates it.
  // Kept here rather than read back from the mapper so that the level
  // survives being switched off, which is what lets a preset stay selected.
  double m_scattering = 0.0;
  bool m_shadowsEnabled = true;
  // Mirrors the mapper's verdict, so a change can be noticed and shown.
  bool m_scatteringOverBudget = false;

  // True while the view's coordinator draws this sink, i.e. its own props
  // are out of the renderer; and whether it was the lead when last told.
  bool m_composited = false;
  bool m_leadApplied = false;

  QPointer<QComboBox> m_scalarsCombo;
  int m_activeScalars = -1;
  // True once nearest-interpolation + lighting have been auto-applied
  // for a LabelMap input on this instance. Latches so user overrides
  // stick across subsequent executions.
  bool m_labelMapDefaultsApplied = false;
};

} // namespace pipeline
} // namespace tomviz

#endif
