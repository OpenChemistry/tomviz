/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumeSink.h"

#include "DoubleSliderWidget.h"
#include "ExplodedGeometry.h"
#include "MultiVolumeCoordinator.h"
#include "ThreadUtils.h"
#include "VolumeBricking.h"
#include "VolumeSinkWidget.h"
#include "vtkNonOrthoImagePlaneWidget.h"

#include "data/VolumeData.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QStyle>
#include <QStyleOptionGroupBox>
#include <QStylePainter>
#include <QJsonArray>
#include <QEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

#include <vtkCallbackCommand.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkLightCollection.h>
#include <vtkPointData.h>
#include <vtkSMProxy.h>
#include <vtkPVRenderView.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPlane.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkMultiBlockVolumeMapper.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkCamera.h>
#include <vtkPlaneCollection.h>
#include <vtkPropCollection.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkProperty.h>
#include <vtkTransform.h>
#include <vtkTrivialProducer.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkTextureObject.h>
#include <vtkVolume.h>
#include <vtkVolumeMapper.h>
#include <vtkVolumeProperty.h>

namespace tomviz {
namespace pipeline {

namespace {

struct LightingPresetValues
{
  bool shade;
  double ambient, diffuse, specular, specularPower;
  double scattering, reach, anisotropy;
  bool smoothNormals;
};

// Indexed by VolumeSink::LightingPreset (Flat..Full), in increasing order
// of render cost. Flat is unlit;
// its remaining values are the Simple baseline so that toggling shading
// back on afterwards gives a sensible look.
//
// Two constraints on the scattering presets, both established by rendering
// the stock nanoparticle sample across each parameter's range:
//
//  - Ambient does nothing once scattering is on. The shader adds it as
//    in_ambient * in_lightAmbientColor, and vtkLight::AmbientColor is black
//    by default, so the term is always zero. Brightness in the scattering
//    path has to come from diffuse, which is why Soft and Full push
//    diffuse to 1.0 and leave ambient at the Simple baseline rather than
//    pretending to raise an "ambient" look.
//
//  - Scattering anisotropy must not be positive. The phase function is
//    evaluated against the view direction, and the lights sit with the
//    camera (ParaView's light kit is made of camera lights, and the
//    headlight that replaces it while volumes render together is one too),
//    so light reaching the camera is backscattered: positive (forward)
//    anisotropy throws it away from the viewer and renders the volume
//    nearly black. Slightly negative reads brightest without the blown-out
//    look that sets in below about -0.5.
//
// Gentle needs its own note, because it gets its look without shadows at
// all. It is for noisy experimental reconstructions, where Simple's specular
// highlight riding on a gradient normal turns reconstruction noise into
// glitter: dropping the highlight and filling the unlit side in with ambient
// measurably lowers the high-frequency energy in the image (about 15% less
// on the stock nanoparticle sample) while keeping the shape readable. That
// is the matte, low-contrast look ChimeraX's preset of the same name gives,
// reached the cheap way - it costs no more to render than Simple.
//
// Reach was the obvious knob to try instead, since ChimeraX gets its gentle
// look by coarsening the ambient-shadow map. It does nothing here. Measured
// on both the nanoparticle sample and a synthetic noisy blob, taking reach
// from Soft's 0.08 up to 1.0 moves the image by well under one 8-bit level:
// the shadow ray saturates almost immediately, so lengthening it finds
// nothing left to occlude.
//
// Which is also why Full shares Soft's reach rather than the 0.2 it shipped
// with. Reach buys nothing visible - 0.08 against 0.2 on Full's own settings
// differs by 0.09 of an 8-bit level - but it does cost render time, because
// a longer shadow ray marches more samples. Under the frame guard that cost
// is not paid in time, it is paid in sharpness: the controller answers an
// expensive frame by sampling more coarsely, so the reach was being spent
// on nothing and taken back out of the picture.
//
// Scattering blend is not a cost lever the same way. The shader casts the
// shadow ray for every shaded sample as soon as the blend is above zero and
// the value only weights the result, so Full's 1.5 costs what Soft's 1.0
// does; it is pure look.
const LightingPresetValues kLightingPresets[] = {
  /* Flat */ { false, 0.1, 0.9, 0.3, 30.0, 0.0, 0.0, 0.0, false },
  /* Simple */ { true, 0.1, 0.9, 0.3, 30.0, 0.0, 0.0, 0.0, false },
  /* Gentle */ { true, 0.35, 0.75, 0.0, 30.0, 0.0, 0.0, 0.0, true },
  /* Soft */ { true, 0.1, 1.0, 0.0, 30.0, 1.0, 0.08, 0.0, true },
  /* Full */ { true, 0.1, 1.0, 0.3, 40.0, 1.5, 0.08, -0.25, true },
};

const int kNumLightingPresets =
  static_cast<int>(sizeof(kLightingPresets) / sizeof(kLightingPresets[0]));

// --- GPU watchdog guard ------------------------------------------------
//
// Volumetric scattering is expensive in a way that is fatal rather than
// merely slow. vtkVolumeShaderComposer emits an unconditional volumeShadow()
// call per light for every shaded sample as soon as
// VolumetricScatteringBlending rises above 0 - the blend value only weights
// the result, it never skips the ray - so each primary sample marches a
// second ray through the volume. On a 256^3 volume at full shadow reach that
// is on the order of 200k samples per pixel instead of ~440.
//
// ParaView gives still renders an effectively unbounded time budget (the
// desired update rate drops to ~0, so VTK's own AutoAdjustSampleDistances
// never engages), and such a frame can run for tens of seconds. Long before
// it finishes the OS decides the GPU has hung and resets it. That is not
// recoverable on macOS: the Metal-backed GL renderer calls abort() when a
// command buffer comes back with kIOGPUCommandBufferCallbackErrorHang, so
// tomviz dies with SIGABRT and no error anyone can catch, and the GPU reset
// takes every other GL/Metal application on the machine down with it.
//
// So bound the frame ourselves rather than trust the driver to survive it,
// trading resolution for safety on the two axes VTK exposes: fewer rays
// (ImageSampleDistance) and coarser steps along each ray (SampleDistance).
//
// Predicting that cost up front does not work. Measured on the stock
// nanoparticle sample, swapping a thresholded opacity map for tomviz's plain
// linear ramp takes one scattering frame from 19 ms to 465 ms, because the
// shader casts a shadow ray for every sample that is not fully transparent.
// The transfer function is user-controlled and edited constantly, so any
// estimate built from the volume's dimensions is wrong by more than an order
// of magnitude in both directions - and guessing high wrecks frames that
// were never in danger.
//
// So measure instead. The first scattering frames of a given configuration
// are rendered deliberately cheaply and timed, and the measurement is used
// to solve for the quality that fits the budget; the picture then sharpens
// over the next frame or two. The very first of those frames is drawn but
// not timed: see DiscardMeasurement.
// Cost falls as the fourth power of the degradation factor (fewer rays in
// each screen axis, and a longer step for both the primary and the shadow
// rays), so the extrapolation is a single pow().
//
// Camera moves are a different matter. A scattering frame cannot be made
// cheaper without changing what it looks like - a coarser ray step brightens
// and thins translucent data, fewer rays blob fine structure - and drawing
// drags at still quality makes heavy data too sluggish to interact with. So
// drags do not approximate the scattering look at all: they render unlit
// (the Flat preset's look), the fastest thing we can draw. Unlit also reads
// closer to the scattering presets than Phong shading does, because those
// presets take their brightness from the diffuse scattering term rather
// than from surface shading - Phong renders them dim and harsh, unlit stays
// bright and even. The still render brings the real look back on release.

// What one still frame should cost. Camera moves render unlit and are not
// guarded at all, so this bounds stills only.
//
// Set against the tightest watchdog we ship under - Windows' TDR default of
// 2 s - leaving room for the estimate to be off by 2x and still not trip it.
// It is also what sets how sharp a still can get: the controller settles at
// reduction = full-quality cost / this, and the sampling knob is its fourth
// root, so a volume that would take 5 s at full quality is ray cast at 1/1.5
// of screen resolution and upscaled. Raising this further buys little
// sharpness for the margin it spends - the fourth root means 2 s would look
// only 16% sharper while sitting exactly on the TDR limit.
const double kTargetFrameSeconds = 1.0;
// Quality is expressed as a cost-reduction factor: 1 is full quality, 100
// means the frame is drawn for a hundredth of what it would otherwise cost.
// It is spent evenly across the two knobs VTK offers, because each distorts
// a different kind of data: thinning the rays (ImageSampleDistance) fattens
// fine structure into blobs on hard-surface opacity maps, while lengthening
// the ray step (SampleDistance) brightens and thins semi-transparent data by
// under-integrating its haze and shadows. At equal cost the even split stays
// closest to the full-quality image on both; either knob alone is worse on
// one of them.
const double kNoReduction = 1.0;
// Per-knob cap: at most 1 ray per 8x8 pixels and one step per 8 voxels.
const double kMaxKnobFactor = 8.0;
// The reduction with both knobs at their cap, each contributing its square.
const double kMaxReduction =
  kMaxKnobFactor * kMaxKnobFactor * kMaxKnobFactor * kMaxKnobFactor;
// First frame of a new configuration, before anything has been timed.
const double kProbeReduction = kMaxReduction;
// Sharpening moves at most this far per frame (the square of a halving on
// each knob), so every gain in quality is backed by a nearby measurement.
const double kMaxSharpeningStep = 16.0;
// Relative change below which the refinement loop stops, so it terminates
// instead of chasing timing noise.
const double kReductionEpsilon = 0.1;

/// What to scale both sampling knobs by to achieve @a reduction. Each knob
/// contributes its square to the total, so an even split gives each the
/// fourth root. The clamp cannot bind while reduction stays within
/// kMaxReduction, and is kept only so the knobs are bounded on their own.
double knobFactor(double reduction)
{
  return reduction > kNoReduction
           ? std::min(kMaxKnobFactor, std::pow(reduction, 0.25))
           : 1.0;
}

/// Everything that invalidates a previous timing measurement. When any of it
/// changes the guard goes back to a probe frame rather than trusting a
/// measurement taken under different conditions.
///
/// Viewport size is deliberately absent: ParaView renders interactive frames
/// into a smaller buffer than still ones, so keying on size would throw the
/// measurement away at the start and end of every drag. Cost scales linearly
/// with pixel count, so the estimate is stored per pixel and rescaled instead.
struct GuardKey
{
  double scattering = -1.0;
  double reach = -1.0;
  int lights = 0;
  // The transfer function is deliberately absent, even though it swings the
  // cost by more than an order of magnitude. Keying on it (via the property's
  // MTime, which folds its functions in) meant every edit threw the estimate
  // away, and an animated curve threw it away on every single frame - so the
  // volume rendered every frame of an animation at maximum coarseness. Its
  // cost is tracked by opacityCost() and folded into the estimate as a scale
  // factor instead. Everything still keyed here changes in steps rather than
  // smoothly, so there is nothing to carry across.
  // Voxel count, not the input's MTime. VolumeData::switchTimeStep swaps in
  // a different vtkImageData for every time step, so keying on identity
  // sends a playing time series back to a probe frame on every frame and
  // pins it at maximum coarseness. Voxel count is what actually sets the
  // cost scale, time steps share it, and a genuinely different dataset
  // still resets. Same-sized data whose content differs is absorbed by the
  // controller instead, since coarsening applies immediately.
  vtkIdType inputPoints = -1;

  bool operator==(const GuardKey& o) const
  {
    return scattering == o.scattering && reach == o.reach &&
           lights == o.lights && inputPoints == o.inputPoints;
  }
  bool operator!=(const GuardKey& o) const { return !(*this == o); }
};

/// Opacity below which a sample is treated as fully transparent, matching
/// what the shader skips.
const double kTransparentOpacity = 1.0 / 255.0;
/// Never let the proxy reach zero: it is used as a ratio.
const double kMinOpacityCost = 1.0 / 256.0;

/// The share of the transfer function's domain that is not transparent.
///
/// This is the part of the transfer function that sets the frame cost - the
/// shader casts a shadow ray for every sample that is not fully transparent -
/// and unlike the function's MTime it moves smoothly as the curve is edited or
/// animated. That is what makes it usable as a scale factor: an estimate
/// measured under one curve can be carried across to the next one instead of
/// being thrown away, which is the difference between a morphing curve
/// refining normally and one that never escapes its probe frame.
///
/// It ignores how the data is distributed across the range, so it is only
/// roughly proportional to the real cost. It does not have to be better than
/// that: it only ever rescales a measurement the controller then corrects, and
/// the correction is applied in the safe direction first.
double opacityCost(vtkPiecewiseFunction* opacity)
{
  if (!opacity) {
    return 1.0;
  }

  double range[2];
  opacity->GetRange(range);
  if (!(range[1] > range[0])) {
    return 1.0;
  }

  const int samples = 256;
  int solid = 0;
  for (int i = 0; i < samples; ++i) {
    const double x =
      range[0] + (range[1] - range[0]) * i / (samples - 1.0);
    if (opacity->GetValue(x) > kTransparentOpacity) {
      ++solid;
    }
  }

  return std::max(kMinOpacityCost, static_cast<double>(solid) / samples);
}

double opacityCost(vtkVolumeProperty* property)
{
  return opacityCost(property ? property->GetScalarOpacity() : nullptr);
}

/// Cost reduction needed to bring a frame of @a secondsPerPixel (the per-pixel
/// cost at full quality) over @a pixels down to @a targetSeconds.
double solveReduction(double secondsPerPixel, double pixels,
                      double targetSeconds)
{
  if (secondsPerPixel <= 0.0 || pixels <= 0.0 || targetSeconds <= 0.0) {
    return kNoReduction;
  }
  const double wanted = secondsPerPixel * pixels / targetSeconds;
  return std::min(kMaxReduction, std::max(kNoReduction, wanted));
}

/// True when @a ren is drawing a frame the user is actively interacting
/// through. This is the same test vtkSmartVolumeMapper::Render uses to
/// decide whether to let the GPU mapper degrade itself.
bool isInteractiveRender(vtkRenderer* ren, double interactiveUpdateRate)
{
  auto* window = ren ? ren->GetRenderWindow() : nullptr;
  return window && window->GetDesiredUpdateRate() >= interactiveUpdateRate;
}

int lightCount(vtkRenderer* ren)
{
  auto* lights = ren ? ren->GetLights() : nullptr;
  return lights ? std::max(1, lights->GetNumberOfItems()) : 1;
}

double viewportPixels(vtkRenderer* ren)
{
  const int* size = ren ? ren->GetSize() : nullptr;
  return size ? static_cast<double>(size[0]) * static_cast<double>(size[1])
              : 0.0;
}

double smallestSpacing(vtkImageData* image)
{
  double spacing[3] = { 1.0, 1.0, 1.0 };
  image->GetSpacing(spacing);
  const double smallest = std::min(std::min(std::abs(spacing[0]),
                                            std::abs(spacing[1])),
                                   std::abs(spacing[2]));
  return smallest > 0.0 ? smallest : 1.0;
}

/// A checkable group box that shrinks to its title row while unchecked.
/// Hiding the body alone leaves the frame, which the style draws from the
/// title down to the bottom of the widget, so an empty box remains. The
/// widget itself is cut off at the title instead (the style option says
/// where that is, since it varies by style) and the frame is left out of
/// the paint so its top edge cannot peek through.
class CollapsingGroupBox : public QGroupBox
{
public:
  using QGroupBox::QGroupBox;

  void setCollapsed(bool collapsed)
  {
    m_collapsed = collapsed;
    // Keep the body from claiming space below the cut-off; a hidden widget
    // still holds the layout's margins.
    if (auto* l = layout()) {
      if (collapsed) {
        l->setContentsMargins(0, 0, 0, 0);
      } else {
        l->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                              style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                              style()->pixelMetric(
                                QStyle::PM_LayoutRightMargin),
                              style()->pixelMetric(
                                QStyle::PM_LayoutBottomMargin));
      }
    }
    if (collapsed) {
      setFixedHeight(titleHeight());
    } else {
      setMinimumHeight(0);
      setMaximumHeight(QWIDGETSIZE_MAX);
    }
    updateGeometry();
    update();
  }

protected:
  // Enter in one of the group's spin boxes commits the value and is then
  // passed up the widget tree, and QGroupBox::event() takes it as a click
  // on the check box, switching the whole feature off. It has to be
  // stopped in event(): the key handlers run after that.
  bool event(QEvent* e) override
  {
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
      const int key = static_cast<QKeyEvent*>(e)->key();
      if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        e->accept();
        return true;
      }
    }
    return QGroupBox::event(e);
  }

  void paintEvent(QPaintEvent* event) override
  {
    if (!m_collapsed) {
      QGroupBox::paintEvent(event);
      return;
    }
    QStylePainter painter(this);
    QStyleOptionGroupBox opt;
    initStyleOption(&opt);
    opt.subControls &= ~QStyle::SC_GroupBoxFrame;
    painter.drawComplexControl(QStyle::CC_GroupBox, opt);
  }

private:
  int titleHeight() const
  {
    QStyleOptionGroupBox opt;
    initStyleOption(&opt);
    const auto label =
      style()->subControlRect(QStyle::CC_GroupBox, &opt,
                              QStyle::SC_GroupBoxLabel, this);
    const auto check =
      style()->subControlRect(QStyle::CC_GroupBox, &opt,
                              QStyle::SC_GroupBoxCheckBox, this);
    return std::max(label.bottom(), check.bottom()) + 1;
  }

  bool m_collapsed = false;
};

} // namespace

// Subclass vtkSmartVolumeMapper so we can forward jittering to the GPU mapper,
// matching the legacy ModuleVolume behavior, and so that Render() can bound
// the cost of a scattering-heavy frame (see the GPU watchdog guard above).
class SmartVolumeMapper : public vtkSmartVolumeMapper
{
public:
  SmartVolumeMapper() { SetRequestedRenderModeToGPU(); }

  static SmartVolumeMapper* New();

  void UseJitteringOn() { GetGPUMapper()->UseJitteringOn(); }
  void UseJitteringOff() { GetGPUMapper()->UseJitteringOff(); }
  vtkTypeBool GetUseJittering() { return GetGPUMapper()->GetUseJittering(); }
  void SetUseJittering(vtkTypeBool b) { GetGPUMapper()->SetUseJittering(b); }

  // Cropping is honoured by whichever mapper actually draws, so set it on
  // the GPU mapper as well as on ourselves (same reason as jittering).
  void SetCroppingState(vtkTypeBool on, const double planes[6], int flags)
  {
    for (vtkVolumeMapper* m : { static_cast<vtkVolumeMapper*>(this),
                                static_cast<vtkVolumeMapper*>(
                                  GetGPUMapper()) }) {
      m->SetCroppingRegionPlanes(const_cast<double*>(planes));
      m->SetCroppingRegionFlags(flags);
      m->SetCropping(on);
    }
  }

  // The scattering level the user asked for. Still frames render it, fitted
  // into the time budget by adjusting the sampling; camera moves render
  // unlit instead (see applyRenderGuard). It only falls back to 0 on stills
  // when even the coarsest possible frame would blow the budget.
  vtkSetClampMacro(RequestedVolumetricScattering, double, 0.0, 2.0);
  vtkGetMacro(RequestedVolumetricScattering, double);

  /// True when the last frame was rendered coarser than the measurement now
  /// says is necessary, i.e. redrawing would produce a sharper picture.
  /// One-shot: the flag is cleared by reading it, so a request cannot outlive
  /// the frame that raised it. Without that, a volume that stops rendering
  /// (hidden, or its view redrawn for some other prop) would leave the flag
  /// set and spin the refinement loop forever.
  bool ConsumeRefinementRequest()
  {
    const bool wanted = NeedsRefinement;
    NeedsRefinement = false;
    return wanted;
  }

  /// Size frames for @a curve rather than for whatever the volume is
  /// showing. Used while exporting, where a controller that tracks the
  /// curve faithfully would vary sharpness from frame to frame and read as
  /// the picture breathing. Null goes back to tracking.
  void SetCostCurveOverride(vtkPiecewiseFunction* curve)
  {
    CostCurveOverride = curve;
  }

  /// True once a frame drawn at maximum coarseness was measured over
  /// budget, so this configuration renders without scattering until
  /// something changes. Not one-shot: it describes a standing state, and
  /// the sink polls it to keep the properties panel honest about why the
  /// shadows are missing.
  bool GetScatteringUnaffordable() const { return ScatteringUnaffordable; }

  /// True when the frame just drawn was an unlit interactive one - i.e. the
  /// scattering look is currently off screen and a still render is owed.
  /// One-shot, cleared by reading, for the same reason as above.
  bool ConsumeSuppressedFrame()
  {
    const bool drawn = SuppressedFrameDrawn;
    SuppressedFrameDrawn = false;
    return drawn;
  }

  // Step every ray at half a voxel instead of letting VTK pick a voxel or
  // more. For a label map the shading normal exists only in the one-voxel
  // boundary shell, so a coarser step lands most first hits past it and
  // draws them unlit; see LabelMapSink.
  void SetFineSampling(bool on) { FineSampling = on; }
  bool GetFineSampling() const { return FineSampling; }

  void Render(vtkRenderer* ren, vtkVolume* vol) override
  {
    const bool guarded = applyRenderGuard(ren, vol);
    vtkSmartVolumeMapper::Render(ren, vol);
    if (guarded) {
      recordFrameTime();
    }
  }

protected:
  /// Returns true if the scattering guard is in force for this frame.
  bool applyRenderGuard(vtkRenderer* ren, vtkVolume* vol)
  {
    NeedsRefinement = false;

    auto* property = vol ? vol->GetProperty() : nullptr;

    // Undo a previous frame's interactive suppression before reading any
    // state off the property.
    if (ShadeSuppressed && property) {
      property->SetShade(1);
      ShadeSuppressed = false;
    }

    const bool shaded = property && property->GetShade() != 0;
    const double requested = shaded ? RequestedVolumetricScattering : 0.0;
    auto* image = vtkImageData::SafeDownCast(GetDataObjectInput());

    if (requested <= 0.0 || !image) {
      SetVolumetricScatteringBlending(0.0f);
      releaseSampleDistances();
      return false;
    }

    // Camera moves render unlit - the Flat look - instead of a degraded
    // scattering frame; see the design notes above. The still frame that
    // follows interaction restores the shading through the block at the top
    // of this function; VolumeSink::onRenderFinished guarantees that still
    // frame actually happens.
    if (isInteractiveRender(ren, GetInteractiveUpdateRate())) {
      property->SetShade(0);
      ShadeSuppressed = true;
      SuppressedFrameDrawn = true;
      SetVolumetricScatteringBlending(0.0f);
      releaseSampleDistances();
      return false;
    }

    const double pixels = viewportPixels(ren);

    GuardKey key;
    key.scattering = requested;
    key.reach = GetGlobalIlluminationReach();
    key.lights = lightCount(ren);
    key.inputPoints = image->GetNumberOfPoints();

    const double cost = CostCurveOverride
                          ? opacityCost(CostCurveOverride.Get())
                          : opacityCost(property);

    if (key != Key) {
      Key = key;
      SecondsPerPixel = -1.0;
      Reduction = kProbeReduction;
      DiscardMeasurement = true;
      ScatteringUnaffordable = false;
      OpacityCost = cost;
    } else if (SecondsPerPixel <= 0.0) {
      // No estimate to scale yet; just track which curve the pending
      // measurement is being taken under, or the first ratio after it
      // would compare that measurement against a curve it never saw.
      OpacityCost = cost;
    } else {
      // Carry the estimate across a changed transfer function instead of
      // discarding it. A curve that just got more opaque scales the estimate
      // up, and coarsening off that lands on this very frame - the safe
      // direction. A cheaper curve scales it down, but sharpening is still
      // rate-limited below, so the gain has to be confirmed by a measurement
      // before it is taken in full.
      if (OpacityCost > 0.0 && cost != OpacityCost) {
        SecondsPerPixel *= cost / OpacityCost;
        ScatteringUnaffordable = false;
      }
      OpacityCost = cost;

      const double wanted =
        solveReduction(SecondsPerPixel, pixels, kTargetFrameSeconds);
      // Coarsening is applied at once - that is the safety direction.
      //
      // Sharpening is capped, but not for the reason it looks like. Fixed
      // per-frame overhead really does dominate a coarse frame's timing,
      // and that biases the estimate pessimistic, never optimistic: a frame
      // at reduction R measures overhead + cost/R, so the full-quality cost
      // this solves for is the true cost plus overhead x R, which can only
      // come out too coarse. The cap is a backstop against a measurement
      // wrong in the other direction - a frame whose cost falls off faster
      // than the sampling model says it should - and it is nearly free:
      // measured across regimes it costs at most one extra frame, and once
      // overhead dominates it never binds at all, because the estimate is
      // already shrinking more slowly than 16x per frame.
      Reduction = wanted > Reduction
                    ? wanted
                    : std::max(wanted, Reduction / kMaxSharpeningStep);
    }

    // Safety valve: when a frame drawn at maximum coarseness still blows the
    // budget, scattering is unaffordable on this configuration - keeping the
    // process alive beats keeping the look.
    //
    // This is deliberately a measured fact about a frame we actually drew,
    // never an extrapolation. A probe frame's time is dominated by fixed
    // per-frame overhead rather than by ray casting - measured here, a
    // knob-8 frame costs the same as a knob-1 one - so scaling it up by
    // kMaxReduction turned "the probe took longer than the budget" into
    // "this volume needs 24 minutes a frame", and fired the valve on volumes
    // that render perfectly well. Worse, the valve returns before
    // recordFrameTime(), so that verdict could never be revised: the volume
    // rendered shaded but shadowless - Simple's look under Full's settings -
    // until something changed the key.
    if (ScatteringUnaffordable) {
      SetVolumetricScatteringBlending(0.0f);
      releaseSampleDistances();
      return false;
    }

    SetVolumetricScatteringBlending(static_cast<float>(requested));

    auto* gpu = GetGPUMapper();
    if (BaseSampleDistance < 0.0f) {
      BaseSampleDistance = gpu->GetSampleDistance();
    }
    // Both flags have to go off on this mapper: vtkSmartVolumeMapper::Render
    // otherwise re-derives the GPU mapper's AutoAdjustSampleDistances from
    // the update rate and hands our explicit distances back to the heuristic.
    SetInteractiveAdjustSampleDistances(0);
    SetAutoAdjustSampleDistances(0);
    const double knob = knobFactor(Reduction);
    gpu->SetImageSampleDistance(static_cast<float>(knob));
    gpu->SetSampleDistance(
      static_cast<float>(knob * smallestSpacing(image)));
    LastPixels = pixels;
    return true;
  }

  /// Fold the frame we just drew into the quality estimate. vtkGPUVolumeRay
  /// CastMapper times its own render; measured against a blocking framebuffer
  /// readback that figure tracks real GPU time to within a few percent here,
  /// so it is a usable signal.
  void recordFrameTime()
  {
    auto* gpu = GetGPUMapper();
    const double measured = gpu->GetTimeToDraw();
    if (measured <= 0.0 || LastPixels <= 0.0) {
      return;
    }
    // The first frame of a configuration also uploads the volume texture and
    // compiles the shader variant that draws it, and both land inside
    // GetTimeToDraw(). Extrapolating that one-time cost by up to 4096x makes
    // the volume look unaffordable: the solved reduction pins at the probe
    // value, so no refinement is requested and the picture is left at
    // maximum coarseness - a visibly blurry frame that only recovers when
    // something else forces a re-measure, such as the still render after a
    // camera move. Draw that frame but do not time it, and ask for one more
    // at the same (safe) coarseness to measure the steady-state cost.
    if (DiscardMeasurement) {
      DiscardMeasurement = false;
      NeedsRefinement = true;
      return;
    }
    // Normalise to what one pixel would cost at full quality, so the estimate
    // survives the viewport changing size - which it does at the start and
    // end of every camera move.
    SecondsPerPixel = measured * Reduction / LastPixels;

    // A frame that measured cheaper than its reduction warranted asks for
    // one redraw to sharpen. Coarsening never schedules anything - the
    // too-slow frame has already happened, and the correction lands on
    // whatever renders next.
    const double wanted =
      solveReduction(SecondsPerPixel, LastPixels, kTargetFrameSeconds);
    NeedsRefinement = wanted < Reduction * (1.0 - kReductionEpsilon);

    // Drawn as coarsely as we know how and still over budget: there is
    // nothing cheaper left to try, so stop drawing scattering for this
    // configuration (see the safety valve above).
    if (Reduction >= kProbeReduction && measured > kTargetFrameSeconds) {
      ScatteringUnaffordable = true;
      NeedsRefinement = false;
    }
  }

  /// Hand sampling back to VTK (or to the fine-sampling policy), undoing
  /// whatever applyRenderGuard() set.
  void releaseSampleDistances()
  {
    if (BaseSampleDistance >= 0.0f) {
      auto* gpu = GetGPUMapper();
      gpu->SetImageSampleDistance(1.0f); // one ray per pixel
      gpu->SetSampleDistance(BaseSampleDistance);
      SetInteractiveAdjustSampleDistances(1);
      SetAutoAdjustSampleDistances(1);
      BaseSampleDistance = -1.0f;
    }
    applySamplingPolicy();
  }

  /// Outside the scattering guard, sampling is VTK's own heuristic unless
  /// fine sampling is on, which pins a half-voxel step for stills and
  /// camera moves alike. Every setter here is a no-op when nothing
  /// changes, so this is safe to run per frame.
  void applySamplingPolicy()
  {
    auto* image = vtkImageData::SafeDownCast(GetDataObjectInput());
    if (!FineSampling || !image) {
      SetInteractiveAdjustSampleDistances(1);
      SetAutoAdjustSampleDistances(1);
      return;
    }
    // Both flags off, or vtkSmartVolumeMapper::Render re-derives the GPU
    // mapper's setting from the update rate (see applyRenderGuard).
    SetInteractiveAdjustSampleDistances(0);
    SetAutoAdjustSampleDistances(0);
    const float step = static_cast<float>(0.5 * smallestSpacing(image));
    SetSampleDistance(step);
    auto* gpu = GetGPUMapper();
    gpu->SetImageSampleDistance(1.0f);
    gpu->SetSampleDistance(step);
  }

  bool FineSampling = false;

  double RequestedVolumetricScattering = 0.0;
  // The mapper's own SampleDistance, stashed while we are overriding it.
  // Negative means we are not currently overriding anything.
  float BaseSampleDistance = -1.0f;
  // Current quality, and the conditions it was measured under.
  double Reduction = kProbeReduction;
  /// opacityCost() as it stood when SecondsPerPixel was last measured, so a
  /// change in the curve can be folded in as a ratio.
  double OpacityCost = -1.0;
  vtkSmartPointer<vtkPiecewiseFunction> CostCurveOverride;
  GuardKey Key;
  bool NeedsRefinement = false;
  // Set when a configuration change sends us back to a probe frame; makes
  // recordFrameTime() skip that frame's one-time costs.
  bool DiscardMeasurement = false;
  // Set once a probe frame has been measured over budget, cleared by any
  // configuration change. While set, frames render without scattering.
  bool ScatteringUnaffordable = false;
  // True while we have shading switched off on the volume property for an
  // interactive frame; the next frame restores it.
  bool ShadeSuppressed = false;
  // Set alongside ShadeSuppressed for each unlit frame; consumed by the
  // sink's render observer to arm the settle timer.
  bool SuppressedFrameDrawn = false;
  // Measured cost of one pixel at full quality; negative until a frame of the
  // current configuration has been timed.
  double SecondsPerPixel = -1.0;
  double LastPixels = 0.0;
};

vtkStandardNewMacro(SmartVolumeMapper)

VolumeSink::VolumeSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Volume");

  // No gradient opacity function: the volume is classified by scalar value
  // alone (see updateColorMap). A function that maps everything to 1 would
  // look the same but cost a gradient per sample. Keep it that way for
  // every VolumeSink: vtkMultiVolume refuses to render a set where some
  // volumes carry a gradient opacity function and others do not.

  m_volumeMapper->SetScalarModeToUsePointFieldData();
  m_volumeMapper->SetBlendMode(vtkVolumeMapper::COMPOSITE_BLEND);
  m_volumeMapper->UseJitteringOn();

  // Mirror the relevant settings onto the bricked mapper so that switching
  // between the two (when a volume crosses the texture-size cap) is seamless.
  // vtkMultiBlockVolumeMapper forwards these to its per-brick sub-mappers and
  // enables ray jittering on each by default. Jittering doubles here as the
  // mechanism that hides any residual brick-seam artifacts.
  m_multiBlockMapper->SetScalarModeToUsePointFieldData();
  m_multiBlockMapper->SetBlendMode(vtkVolumeMapper::COMPOSITE_BLEND);

  m_volumeProperty->SetInterpolationType(VTK_LINEAR_INTERPOLATION);
  // Start on the Simple preset: directional shading reads the shape of a
  // volume far better than the unlit Flat look, and it costs nothing extra.
  // Saved state always carries lighting/enabled explicitly, so this only
  // affects newly created volumes.
  const auto& simple = kLightingPresets[static_cast<int>(
    LightingPreset::Simple)];
  m_volumeProperty->SetShade(simple.shade ? 1 : 0);
  m_volumeProperty->SetAmbient(simple.ambient);
  m_volumeProperty->SetDiffuse(simple.diffuse);
  m_volumeProperty->SetSpecular(simple.specular);
  m_volumeProperty->SetSpecularPower(simple.specularPower);

  m_volume->SetMapper(m_volumeMapper);
  m_volume->SetVisibility(0);
  m_volume->SetProperty(m_volumeProperty);

  // Fallback still render after interaction; see onRenderFinished(). The
  // interval only needs to outlast the gap between interactive frames -
  // even heavy unlit frames arrive well inside 400 ms.
  m_settleTimer.setSingleShot(true);
  m_settleTimer.setInterval(400);
  connect(&m_settleTimer, &QTimer::timeout, this,
          [this]() { emit renderNeeded(); });
}

VolumeSink::~VolumeSink()
{
  finalize();
}

QIcon VolumeSink::icon() const
{
  return QIcon(QStringLiteral(":/icons/pqVolumeData.png"));
}

void VolumeSink::setFineSampling(bool enabled)
{
  for (auto* mapper : allMappers()) {
    mapper->SetFineSampling(enabled);
  }
  if (auto* coordinator = MultiVolumeCoordinator::find(view())) {
    coordinator->refreshSettings();
  }
  emit renderNeeded();
}

bool VolumeSink::fineSampling() const
{
  return m_volumeMapper->GetFineSampling();
}

void VolumeSink::setVisibility(bool visible)
{
  // Only show the prop once the mapper actually has data. A visible volume
  // whose mapper has no input makes vtkVolume::RenderVolumetricGeometry()
  // call Update() on it before its own "no input, return silently" check,
  // which prints a spurious "Input port 0 ... has 0 connections but is not
  // optional" error on every render. That window is real: deserialize()
  // restores visibility at state load, before the (threaded) pipeline has
  // executed. consume() re-applies visibility() once data arrives.
  auto* mapper = m_volume->GetMapper();
  bool hasInput = mapper && mapper->GetDataObjectInput();
  bool shown = visible && hasInput && volumeRenderingEnabled();
  for (auto* slab : allVolumes()) {
    slab->SetVisibility(shown ? 1 : 0);
  }
  LegacyModuleSink::setVisibility(visible);
  // VTK ignores the visibility of the volumes inside a vtkMultiVolume, so
  // a hidden sink has to leave the set rather than sit in it invisibly.
  syncMultiVolumeMembership();
  updateExplodedWidget();
}

bool VolumeSink::isColorMapNeeded() const
{
  return true;
}

bool VolumeSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  // A fresh view starts the sink on its own props; the view's coordinator
  // takes them over below if it already draws another volume.
  m_composited = false;
  m_leadApplied = false;
  renderView()->AddPropToRenderer(m_volume);
  for (auto& slab : m_explodedVolumes) {
    renderView()->AddPropToRenderer(slab);
  }
  m_explodedOrderDirty = true;
  // Slabs composite in prop order, so re-order them before every render
  if (auto* renderer = renderView()->GetRenderer()) {
    m_sortObserver->SetClientData(this);
    m_sortObserver->SetCallback(
      [](vtkObject*, unsigned long, void* clientData, void*) {
        static_cast<VolumeSink*>(clientData)->sortExplodedProps();
      });
    m_sortObserverId =
      renderer->AddObserver(vtkCommand::StartEvent, m_sortObserver);
  }

  // The scattering guard draws its first frame of any new configuration
  // deliberately coarse so it can time it safely. Watch for the end of each
  // render so a sharper one can be requested once that measurement says
  // there is headroom.
  if (auto* window = renderView()->GetRenderWindow()) {
    m_refinementObserver->SetClientData(this);
    m_refinementObserver->SetCallback(
      [](vtkObject*, unsigned long, void* clientData, void*) {
        static_cast<VolumeSink*>(clientData)->onRenderFinished();
      });
    m_refinementObserverId =
      window->AddObserver(vtkCommand::EndEvent, m_refinementObserver);
  }
  syncMultiVolumeMembership();
  return true;
}

void VolumeSink::onRenderFinished()
{
  if (m_usingMultiBlock) {
    return;
  }

  // The guard can decide mid-render that scattering is unaffordable here.
  // Nothing else would tell the user why the shadows just went away, so
  // push the change into the panel.
  const bool overBudget = m_volumeMapper->GetScatteringUnaffordable();
  if (overBudget != m_scatteringOverBudget) {
    m_scatteringOverBudget = overBudget;
    emit lightingStateChanged();
  }

  bool suppressed = false;
  bool refine = false;
  for (auto* mapper : allMappers()) {
    suppressed |= mapper->ConsumeSuppressedFrame();
    refine |= mapper->ConsumeRefinementRequest();
  }
  if (suppressed) {
    // An unlit interactive frame is on screen. In principle ParaView follows
    // interaction with a still render, which is what brings the scattering
    // look back - but that depends on an EndInteractionEvent reaching the
    // view's interactor helper, and macOS trackpad wheel and gesture streams
    // do not reliably deliver one. Arm a fallback: each interactive frame
    // re-arms the timer, so it fires only once the motion has genuinely
    // stopped, and the still frame it requests is what restores the look. A
    // still frame that arrives on its own cancels it below.
    m_settleTimer.start();
    return;
  }
  m_settleTimer.stop();

  if (!refine) {
    return;
  }
  // Queued: we are inside the render that just finished, and re-entering the
  // render window from its own EndEvent would recurse. The loop terminates
  // because each pass either lands within kReductionEpsilon of the measured
  // target or stops improving.
  QTimer::singleShot(0, this, [this]() { emit renderNeeded(); });
}

bool VolumeSink::finalize()
{
  // Leave the view's multi-volume first: that hands the sink its own props
  // back, which the removal below then takes out of the renderer.
  if (auto* coordinator = MultiVolumeCoordinator::find(view())) {
    coordinator->removeMember(this);
  }
  m_composited = false;
  m_leadApplied = false;
  if (renderView()) {
    if (m_refinementObserverId) {
      if (auto* window = renderView()->GetRenderWindow()) {
        window->RemoveObserver(m_refinementObserverId);
      }
      m_refinementObserverId = 0;
    }
    if (m_sortObserverId) {
      if (auto* renderer = renderView()->GetRenderer()) {
        renderer->RemoveObserver(m_sortObserverId);
      }
      m_sortObserverId = 0;
    }
    renderView()->RemovePropFromRenderer(m_volume);
    for (auto& slab : m_explodedVolumes) {
      renderView()->RemovePropFromRenderer(slab);
    }
  }
  if (m_explodedWidget) {
    if (m_explodedWidgetTag) {
      m_explodedWidget->RemoveObserver(m_explodedWidgetTag);
      m_explodedWidgetTag = 0;
    }
    if (m_explodedWidgetEndTag) {
      m_explodedWidget->RemoveObserver(m_explodedWidgetEndTag);
      m_explodedWidgetEndTag = 0;
    }
    // InteractionOff/Off need the interactor, so before it is cleared
    if (m_explodedWidget->GetInteractor() && m_explodedWidget->GetEnabled()) {
      m_explodedWidget->InteractionOff();
      m_explodedWidget->Off();
    }
    m_explodedWidget->SetInteractor(nullptr);
    m_explodedWidget = nullptr;
    m_explodedWidgetImage = nullptr;
  }
  return LegacyModuleSink::finalize();
}

void VolumeSink::updateExplodedWidget()
{
  auto vol = volumeData();
  // Not while the view's multi-volume draws this sink either: the
  // exploded view has no effect there.
  const bool show = m_explodedEnabled && m_explodedShowArrow &&
                    m_explodedAxis == kExplodedCustomAxis && visibility() &&
                    !m_usingMultiBlock && !m_composited && vol &&
                    vol->isValid() &&
                    renderView() && renderView()->GetRenderWindow() &&
                    renderView()->GetRenderWindow()->GetInteractor();
  if (!show) {
    if (m_explodedWidget && m_explodedWidget->GetInteractor() &&
        m_explodedWidget->GetEnabled()) {
      m_explodedWidget->Off();
    }
    return;
  }

  if (!m_explodedWidget) {
    // The clip's plane widget with its plane hidden: only the arrow is
    // left, and dragging its tip turns the normal, which is the
    // direction. The plane's outline is kept at zero opacity so it is
    // still there to grab, which slides the widget; the end of a drag
    // puts it back at the center.
    m_explodedWidget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();
    m_explodedWidget->SetInteractor(
      renderView()->GetRenderWindow()->GetInteractor());
    m_explodedWidget->TextureVisibilityOff();
    m_explodedWidget->GetPlaneProperty()->SetOpacity(0.0);
    m_explodedWidget->GetSelectedPlaneProperty()->SetOpacity(0.0);
    vtkNew<vtkCallbackCommand> callback;
    callback->SetClientData(this);
    callback->SetCallback(
      [](vtkObject*, unsigned long, void* clientData, void*) {
        static_cast<VolumeSink*>(clientData)->onExplodedWidgetInteraction();
      });
    m_explodedWidgetTag =
      m_explodedWidget->AddObserver(vtkCommand::InteractionEvent, callback);
    vtkNew<vtkCallbackCommand> ended;
    ended->SetClientData(this);
    ended->SetCallback([](vtkObject*, unsigned long, void* clientData,
                          void*) {
      static_cast<VolumeSink*>(clientData)->onExplodedWidgetInteractionEnded();
    });
    m_explodedWidgetEndTag = m_explodedWidget->AddObserver(
      vtkCommand::EndInteractionEvent, ended);
  }

  if (m_explodedWidgetDragging) {
    return;
  }
  auto* image = vol->imageData();
  if (m_explodedWidgetImage != image) {
    vtkNew<vtkTrivialProducer> producer;
    producer->SetOutput(image);
    m_explodedWidget->SetInputConnection(producer->GetOutputPort());
    m_explodedWidgetImage = image;
  }
  double bounds[6];
  image->GetBounds(bounds);
  double center[3] = { (bounds[0] + bounds[1]) / 2.0,
                       (bounds[2] + bounds[3]) / 2.0,
                       (bounds[4] + bounds[5]) / 2.0 };
  const auto dir = explodedUnitDirection();
  double normal[3] = { dir[0], dir[1], dir[2] };
  m_explodedWidget->SetPlaneOrientation(-1);
  m_explodedWidget->SetCenter(center);
  m_explodedWidget->SetNormal(normal);
  m_explodedWidget->UpdatePlacement();
  if (!m_explodedWidget->GetEnabled()) {
    m_explodedWidget->On();
    m_explodedWidget->InteractionOn();
  }
  m_explodedWidget->SetArrowVisibility(1);
}

void VolumeSink::onExplodedWidgetInteraction()
{
  if (!m_explodedWidget) {
    return;
  }
  double normal[3];
  m_explodedWidget->GetNormal(normal);
  // The widget is being dragged: it must not be re-placed from under the
  // drag by the update the new direction triggers
  m_explodedWidgetDragging = true;
  setExplodedDirection(normal[0], normal[1], normal[2]);
  m_explodedWidgetDragging = false;
}

void VolumeSink::onExplodedWidgetInteractionEnded()
{
  // Dragging the (invisible) plane slides the widget; only its
  // direction means anything, so put it back at the center.
  updateExplodedWidget();
  emit renderNeeded();
}

void VolumeSink::clearVisualization()
{
  for (auto* slab : allVolumes()) {
    slab->SetVisibility(0);
  }
  if (m_explodedWidget && m_explodedWidget->GetInteractor() &&
      m_explodedWidget->GetEnabled()) {
    m_explodedWidget->Off();
  }
  syncMultiVolumeMembership();
}

bool VolumeSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  // First-time setup for a LabelMap input: linear interpolation
  // smears across discrete label boundaries (visually wrong) and the
  // unlit volumetric look hides 3-D structure of segmentation regions.
  // Flip both to sensible defaults on the first encounter and latch so
  // user overrides stick.
  if (!m_labelMapDefaultsApplied &&
      inputs["volume"].type() == PortType::LabelMap) {
    setInterpolationType(VTK_NEAREST_INTERPOLATION);
    setLighting(true);
    m_labelMapDefaultsApplied = true;
  }

  updateMapperForInput(volume->imageData());
  applyActiveScalars();
  // Re-derive the cut planes: the new data may have different bounds.
  applyCutOut();
  applyExploded();
  bool shown = visibility() && volumeRenderingEnabled();
  for (auto* slab : allVolumes()) {
    slab->SetVisibility(shown ? 1 : 0);
  }
  // Data is what makes the sink eligible for the view's multi-volume (and
  // bricking, decided above, is what rules it out).
  syncMultiVolumeMembership();

  onMetadataChanged();
  return true;
}

int VolumeSink::maxTextureSize() const
{
  // GL queries (and MakeCurrent below) must run on the thread that owns the
  // render window's GL context - the GUI thread. This runs inside consume(),
  // which VolumeSink runs on the GUI thread (consumeOnGuiThread()), so no
  // marshaling is needed; the assert pins that contract.
  TOMVIZ_ASSERT_GUI_THREAD();
  if (renderView()) {
    if (auto* glRW = vtkOpenGLRenderWindow::SafeDownCast(
          renderView()->GetRenderWindow())) {
      // GetMaximumTextureSize3D only returns a value when the GL context is
      // current; consume() usually runs outside a render, so make the window
      // current first. This avoids over-bricking on high-limit GPUs (e.g.
      // NVIDIA reports 16384) when we would otherwise hit the fallback.
      if (glRW->GetNeverRendered() == 0) {
        glRW->MakeCurrent();
      }
      int maxSize = vtkTextureObject::GetMaximumTextureSize3D(glRW);
      if (maxSize > 0) {
        return maxSize;
      }
    }
  }
  // No usable GL context yet (e.g. the view has not rendered once). 2048 is
  // the smallest GL_MAX_3D_TEXTURE_SIZE we expect to encounter, so bricking
  // to it is always safe for correctness; on a higher-limit GPU it just means
  // we may brick a volume that would have fit, until the next data update
  // re-queries the now-current context.
  return 2048;
}

void VolumeSink::updateMapperForInput(vtkImageData* image)
{
  const int maxTex = maxTextureSize();
  if (exceedsTextureLimit(image, maxTex)) {
    // Too big for a single 3-D texture: brick it and render with resident
    // per-brick textures. Holding m_brickedVolume keeps the bricks alive.
    m_brickedVolume = brickVolume(image, maxTex);
    m_multiBlockMapper->SetInputDataObject(m_brickedVolume);
    if (!m_usingMultiBlock) {
      m_volume->SetMapper(m_multiBlockMapper);
      m_usingMultiBlock = true;
      // A clip set up while the volume still fit in one texture stops having
      // any effect now that we render in bricks - tell the user why.
      auto* planes = m_volumeMapper->GetClippingPlanes();
      if (planes && planes->GetNumberOfItems() > 0) {
        warnClippingUnsupported();
      }
      // Same for a scattering preset: it is unbounded on the bricked path, so
      // fall back to Simple rather than leave a preset half-applied.
      if (effectiveScattering() > 0.0) {
        warnScatteringUnsupported();
        applyLightingPreset(LightingPreset::Simple);
      }
      emit lightingStateChanged();
    }
  } else {
    // Fits in one texture: keep the original single-mapper path so normal
    // volumes render exactly as before, with no bricking overhead.
    m_volumeMapper->SetInputData(image);
    m_brickedVolume = nullptr;
    if (m_usingMultiBlock) {
      m_volume->SetMapper(m_volumeMapper);
      m_usingMultiBlock = false;
      // The scattering presets are available again on this path.
      emit lightingStateChanged();
    }
  }
}

void VolumeSink::updateColorMap()
{
  auto* cmap = colorMap();
  if (!cmap) {
    return;
  }
  auto* ctf = vtkColorTransferFunction::SafeDownCast(
    cmap->GetClientSideObject());
  auto* omap = opacityMap();
  auto* opacity = omap ? vtkPiecewiseFunction::SafeDownCast(
                           omap->GetClientSideObject())
                       : nullptr;

  if (ctf) {
    m_volumeProperty->SetColor(ctf);
  }
  // An animation supplies its own blended curve while it runs, so that the
  // one the user authored in the histogram editor is still there when it
  // stops.
  if (m_animatedScalarOpacity) {
    m_volumeProperty->SetScalarOpacity(m_animatedScalarOpacity);
  } else if (opacity) {
    m_volumeProperty->SetScalarOpacity(opacity);
  }

  // The dataset's gradient-opacity curve (gradientOpacity()) is deliberately
  // not applied. Legacy ModuleVolume only used it in its GRADIENT_1D and
  // GRADIENT_2D transfer modes, which VolumeSink does not expose, and a
  // state file can carry a curve whose range was calibrated for other data,
  // which would make the volume appear invisible. TODO: apply
  // gradientOpacity() when a transfer mode selector is added to VolumeSink.

  emit renderNeeded();
}

// --- Lighting ---

bool VolumeSink::lighting() const
{
  return m_volumeProperty->GetShade() != 0;
}

void VolumeSink::setLighting(bool enabled)
{
  m_volumeProperty->SetShade(enabled ? 1 : 0);
  emit lightingChanged(enabled);
  emit lightingStateChanged();
  emit renderNeeded();
}

double VolumeSink::ambient() const
{
  return m_volumeProperty->GetAmbient();
}

void VolumeSink::setAmbient(double value)
{
  m_volumeProperty->SetAmbient(value);
  emit lightingStateChanged();
  emit renderNeeded();
}

double VolumeSink::diffuse() const
{
  return m_volumeProperty->GetDiffuse();
}

void VolumeSink::setDiffuse(double value)
{
  m_volumeProperty->SetDiffuse(value);
  emit lightingStateChanged();
  emit renderNeeded();
}

double VolumeSink::specular() const
{
  return m_volumeProperty->GetSpecular();
}

void VolumeSink::setSpecular(double value)
{
  m_volumeProperty->SetSpecular(value);
  emit lightingStateChanged();
  emit renderNeeded();
}

double VolumeSink::specularPower() const
{
  return m_volumeProperty->GetSpecularPower();
}

void VolumeSink::setSpecularPower(double value)
{
  m_volumeProperty->SetSpecularPower(value);
  emit lightingStateChanged();
  emit renderNeeded();
}

double VolumeSink::volumetricScattering() const
{
  // The requested value, not the mapper's current one - the mappers lower
  // theirs during interaction and raise it again for the still frame, and
  // the shadow switch can be holding it at zero.
  return m_scattering;
}

void VolumeSink::setVolumetricScattering(double value)
{
  if (value > 0.0 && !scatteringSupported()) {
    warnScatteringUnsupported();
    value = 0.0;
  }
  m_scattering = value;
  applyScattering();
  emit lightingStateChanged();
  emit renderNeeded();
}

bool VolumeSink::shadowsEnabled() const
{
  return m_shadowsEnabled;
}

bool VolumeSink::scatteringOverBudget() const
{
  return m_scatteringOverBudget;
}

void VolumeSink::setShadowsEnabled(bool enabled)
{
  if (enabled && m_scattering > 0.0 && !scatteringSupported()) {
    warnScatteringUnsupported();
    enabled = false;
  }
  m_shadowsEnabled = enabled;
  applyScattering();
  emit lightingStateChanged();
  emit renderNeeded();
}

double VolumeSink::effectiveScattering() const
{
  return m_shadowsEnabled ? m_scattering : 0.0;
}

void VolumeSink::applyScattering()
{
  // Deliberately not forwarded to m_multiBlockMapper: its per-brick mappers
  // are out of reach, so a scattering frame there cannot be bounded. Leaving
  // that mapper at its zero default is the backstop that guarantees the
  // bricked path never renders one, whatever state arrives.
  // Each exploded slab would budget a full frame of scattering on its own,
  // so shadows stay off while the view is exploded (the extra slabs never
  // leave zero).
  m_volumeMapper->SetRequestedVolumetricScattering(
    m_explodedEnabled ? 0.0 : effectiveScattering());
}

bool VolumeSink::scatteringSupported() const
{
  // A volume too large for one 3-D texture renders through
  // vtkMultiBlockVolumeMapper, whose per-brick mappers are private - there is
  // no way to cap their sampling, so a scattering frame there is unbounded.
  // These are also the largest volumes, i.e. the likeliest to hang the GPU.
  if (m_usingMultiBlock || m_explodedEnabled) {
    return false;
  }
  // Escape hatch for sites deploying to hardware known not to cope, or for
  // anyone who has already lost a session to it.
  auto* core = pqApplicationCore::instance();
  auto* settings = core ? core->settings() : nullptr;
  return !settings ||
         settings->value("Volume.AllowVolumetricScattering", true).toBool();
}

void VolumeSink::warnScatteringUnsupported() const
{
  if (m_usingMultiBlock) {
    qWarning("VolumeSink: volumetric scattering is not supported for volumes "
             "larger than the GPU's 3-D texture size limit. This volume is "
             "rendered in bricks, whose sampling cannot be bounded, so the "
             "Soft and Full lighting presets are unavailable. Reduce the "
             "volume below the limit (e.g. subsample or crop) to use them.");
    return;
  }
  qWarning("VolumeSink: volumetric scattering is disabled by the "
           "Volume.AllowVolumetricScattering setting.");
}

double VolumeSink::shadowReach() const
{
  return m_volumeMapper->GetGlobalIlluminationReach();
}

void VolumeSink::setShadowReach(double value)
{
  for (auto* mapper : allMappers()) {
    mapper->SetGlobalIlluminationReach(value);
  }
  m_multiBlockMapper->SetGlobalIlluminationReach(value);
  emit lightingStateChanged();
  emit renderNeeded();
}

double VolumeSink::scatteringAnisotropy() const
{
  return m_volumeProperty->GetScatteringAnisotropy();
}

void VolumeSink::setScatteringAnisotropy(double value)
{
  m_volumeProperty->SetScatteringAnisotropy(value);
  emit lightingStateChanged();
  emit renderNeeded();
}

bool VolumeSink::smoothNormals() const
{
  return m_volumeMapper->GetComputeNormalFromOpacity();
}

double VolumeSink::opacityRenderCost(vtkPiecewiseFunction* curve)
{
  return opacityCost(curve);
}

void VolumeSink::setWorstCaseOpacity(vtkPiecewiseFunction* curve)
{
  for (auto* mapper : allMappers()) {
    mapper->SetCostCurveOverride(curve);
  }
}

void VolumeSink::setAnimatedScalarOpacity(vtkPiecewiseFunction* opacity)
{
  m_animatedScalarOpacity = opacity;
  updateColorMap();
  emit renderNeeded();
}

void VolumeSink::setSmoothNormals(bool enabled)
{
  for (auto* mapper : allMappers()) {
    mapper->SetComputeNormalFromOpacity(enabled);
  }
  m_multiBlockMapper->SetComputeNormalFromOpacity(enabled);
  // A mapper setting, so on the shared mapper it is the lead's that counts.
  if (auto* coordinator = MultiVolumeCoordinator::find(view())) {
    coordinator->refreshSettings();
  }
  emit lightingStateChanged();
  emit renderNeeded();
}

// --- Lighting presets ---

void VolumeSink::applyLightingPreset(LightingPreset preset)
{
  int idx = static_cast<int>(preset);
  if (idx < 0 || idx >= kNumLightingPresets) {
    return;
  }
  const auto& p = kLightingPresets[idx];
  // Choosing a preset that casts shadows is a request to see them, so it
  // undoes an earlier flip of the shadow switch.
  if (p.scattering > 0.0) {
    setShadowsEnabled(true);
  }
  setAmbient(p.ambient);
  setDiffuse(p.diffuse);
  setSpecular(p.specular);
  setSpecularPower(p.specularPower);
  setVolumetricScattering(p.scattering);
  setShadowReach(p.reach);
  setScatteringAnisotropy(p.anisotropy);
  setSmoothNormals(p.smoothNormals);
  setLighting(p.shade);
}

UserLightingPreset VolumeSink::currentLightingValues() const
{
  UserLightingPreset p;
  p.shade = lighting();
  p.ambient = ambient();
  p.diffuse = diffuse();
  p.specular = specular();
  p.specularPower = specularPower();
  // What is actually rendering: a level left behind with shadows off
  // must not switch them back on when the preset is applied
  p.scattering = effectiveScattering();
  p.reach = shadowReach();
  p.anisotropy = scatteringAnisotropy();
  p.smoothNormals = smoothNormals();
  return p;
}

void VolumeSink::applyUserLightingPreset(const UserLightingPreset& p)
{
  if (p.scattering > 0.0) {
    setShadowsEnabled(true);
  }
  setAmbient(p.ambient);
  setDiffuse(p.diffuse);
  setSpecular(p.specular);
  setSpecularPower(p.specularPower);
  setVolumetricScattering(p.scattering);
  setShadowReach(p.reach);
  setScatteringAnisotropy(p.anisotropy);
  setSmoothNormals(p.smoothNormals);
  setLighting(p.shade);
}

QString VolumeSink::matchingUserLightingPreset() const
{
  auto current = currentLightingValues();
  for (const auto& preset : LightingPresetStore::instance().presets()) {
    if (preset.matches(current)) {
      return preset.name;
    }
  }
  return QString();
}

VolumeSink::LightingPreset VolumeSink::currentLightingPreset() const
{
  // With shading off none of the other parameters affect the render, so any
  // unlit state is Flat - including pre-preset defaults and old state files.
  if (!lighting()) {
    return LightingPreset::Flat;
  }
  // Not named "near": that is a legacy macro in the Windows headers.
  auto approx = [](double a, double b) { return std::fabs(a - b) < 1e-3; };
  for (int i = 1; i < kNumLightingPresets; ++i) {
    const auto& p = kLightingPresets[i];
    if (approx(ambient(), p.ambient) && approx(diffuse(), p.diffuse) &&
        approx(specular(), p.specular) &&
        approx(specularPower(), p.specularPower) &&
        approx(volumetricScattering(), p.scattering) &&
        approx(shadowReach(), p.reach) &&
        approx(scatteringAnisotropy(), p.anisotropy) &&
        smoothNormals() == p.smoothNormals) {
      return static_cast<LightingPreset>(i);
    }
  }
  return LightingPreset::Custom;
}

// --- Blending ---

int VolumeSink::blendingMode() const
{
  return m_volumeMapper->GetBlendMode();
}

void VolumeSink::setBlendingMode(int mode)
{
  // Keep both mappers in sync so the setting survives a switch between them.
  for (auto* mapper : allMappers()) {
    mapper->SetBlendMode(mode);
  }
  m_multiBlockMapper->SetBlendMode(mode);
  emit renderNeeded();
}

// --- Interpolation ---

int VolumeSink::interpolationType() const
{
  return m_volumeProperty->GetInterpolationType();
}

void VolumeSink::setInterpolationType(int type)
{
  m_volumeProperty->SetInterpolationType(type);
  emit interpolationTypeChanged(type);
  emit renderNeeded();
}

// --- Jittering ---

bool VolumeSink::jittering() const
{
  return m_volumeMapper->GetUseJittering() != 0;
}

void VolumeSink::setJittering(bool enabled)
{
  // Applies to the single-texture path only. vtkMultiBlockVolumeMapper forces
  // jittering on for each brick and exposes no way to change it, nor any access
  // to its per-brick mappers, so the toggle has no effect on bricked
  // (over-2048) volumes - which is the safe default, since jittering also helps
  // hide brick seams.
  for (auto* mapper : allMappers()) {
    mapper->SetUseJittering(enabled ? 1 : 0);
  }
  emit renderNeeded();
}

// --- Solidity ---

double VolumeSink::solidity() const
{
  return 1.0 / m_volumeProperty->GetScalarOpacityUnitDistance();
}

void VolumeSink::setSolidity(double value)
{
  if (value > 0.0) {
    m_volumeProperty->SetScalarOpacityUnitDistance(1.0 / value);
    emit solidityChanged(value);
    emit renderNeeded();
  }
}

// --- Active scalars ---

int VolumeSink::activeScalars() const
{
  return m_activeScalars;
}

void VolumeSink::setActiveScalars(int index)
{
  m_activeScalars = index;
  applyActiveScalars();
  emit renderNeeded();
}

void VolumeSink::applyActiveScalars()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  // A name-based selection saved by legacy (or a previous deserialize
  // that fired before data was available) gets resolved here now that
  // we have the VolumeData in hand.
  resolvePendingActiveScalar(m_activeScalars);

  auto* pointData = vol->imageData()->GetPointData();
  int idx = m_activeScalars;
  vtkDataArray* selected = nullptr;
  if (idx >= 0 && idx < pointData->GetNumberOfArrays()) {
    selected = pointData->GetArray(idx);
  }
  if (!selected) {
    // Default (idx < 0) or saved index doesn't fit the current data —
    // fall back to PointData's active. m_activeScalars is left alone
    // so the user's selection snaps back on the next layout match.
    selected = pointData->GetScalars();
  }
  if (selected && selected->GetName()) {
    for (auto* mapper : allMappers()) {
      mapper->SelectScalarArray(selected->GetName());
    }
    m_multiBlockMapper->SelectScalarArray(selected->GetName());
  }
  // The shared mapper reads the selection off the input it is handed.
  if (auto* coordinator = MultiVolumeCoordinator::find(view())) {
    coordinator->refreshInput(this);
  }
}

// --- Clipping ---

void VolumeSink::warnClippingUnsupported() const
{
  qWarning("VolumeSink: clipping is not supported for volumes larger than the "
           "GPU's 3-D texture size limit. This volume is rendered in bricks, so "
           "the clipping plane will have no effect. Reduce the volume below the "
           "limit (e.g. subsample or crop) to use clipping.");
}

// NOTE: Clipping planes apply to the single-texture path only. VTK 9.6's
// vtkMultiBlockVolumeMapper does not forward clipping planes to its per-brick
// mappers and exposes no access to them, so clipping has no effect on bricked
// (over-2048) volumes. The planes are still tracked on m_volumeMapper so that
// clipping works as soon as the volume drops back under the texture-size cap.
// TODO(3.x): to clip bricked volumes we would need to manage the per-brick
// mappers ourselves rather than delegating to vtkMultiBlockVolumeMapper.
void VolumeSink::addClippingPlane(vtkPlane* plane)
{
  if (plane) {
    for (auto* mapper : allMappers()) {
      mapper->AddClippingPlane(plane);
    }
    if (m_usingMultiBlock) {
      warnClippingUnsupported();
    }
    if (auto* coordinator = MultiVolumeCoordinator::find(view())) {
      coordinator->refreshSettings();
    }
    emit renderNeeded();
  }
}

void VolumeSink::removeClippingPlane(vtkPlane* plane)
{
  if (plane) {
    for (auto* mapper : allMappers()) {
      mapper->RemoveClippingPlane(plane);
    }
    if (auto* coordinator = MultiVolumeCoordinator::find(view())) {
      coordinator->refreshSettings();
    }
    emit renderNeeded();
  }
}

void VolumeSink::removeAllClippingPlanes()
{
  for (auto* mapper : allMappers()) {
    mapper->RemoveAllClippingPlanes();
  }
  if (auto* coordinator = MultiVolumeCoordinator::find(view())) {
    coordinator->refreshSettings();
  }
  emit renderNeeded();
}

vtkPlaneCollection* VolumeSink::clippingPlanes() const
{
  return m_volumeMapper->GetClippingPlanes();
}

// --- Cut-out ---

bool VolumeSink::cutOutEnabled() const
{
  return m_cutOutEnabled;
}

void VolumeSink::setCutOutEnabled(bool enabled)
{
  if (m_cutOutEnabled == enabled) {
    return;
  }
  m_cutOutEnabled = enabled;
  if (enabled && m_explodedEnabled) {
    // The two own the mapper's cropping; only one can be on. Not a user
    // request to reframe, so leave the camera alone.
    setExplodedEnabledInternal(false, /*refitCamera=*/false);
  }
  applyCutOut();
  emit cutOutChanged();
  emit renderNeeded();
}

int VolumeSink::cutOutCorner() const
{
  return m_cutOutCorner;
}

void VolumeSink::setCutOutCorner(int corner)
{
  corner = qBound(0, corner, 7);
  if (m_cutOutCorner == corner) {
    return;
  }
  m_cutOutCorner = corner;
  applyCutOut();
  emit cutOutChanged();
  emit renderNeeded();
}

double VolumeSink::cutOutPosition(int axis) const
{
  if (axis < 0 || axis > 2) {
    return 0.5;
  }
  return m_cutOutPosition[axis];
}

void VolumeSink::setCutOutPosition(int axis, double fraction)
{
  if (axis < 0 || axis > 2) {
    return;
  }
  fraction = qBound(0.0, fraction, 1.0);
  if (m_cutOutPosition[axis] == fraction) {
    return;
  }
  m_cutOutPosition[axis] = fraction;
  applyCutOut();
  emit renderNeeded();
}

std::vector<SmartVolumeMapper*> VolumeSink::allMappers()
{
  std::vector<SmartVolumeMapper*> mappers{ m_volumeMapper.Get() };
  for (auto& mapper : m_explodedMappers) {
    mappers.push_back(mapper);
  }
  return mappers;
}

std::vector<vtkVolume*> VolumeSink::allVolumes()
{
  std::vector<vtkVolume*> volumes{ m_volume.Get() };
  for (auto& slab : m_explodedVolumes) {
    volumes.push_back(slab);
  }
  return volumes;
}

bool VolumeSink::explodedEnabled() const
{
  return m_explodedEnabled;
}

void VolumeSink::setExplodedEnabled(bool enabled, bool refitCamera)
{
  setExplodedEnabledInternal(enabled, refitCamera);
}

void VolumeSink::setExplodedEnabledInternal(bool enabled, bool refitCamera)
{
  if (m_explodedEnabled == enabled) {
    return;
  }
  if (enabled && m_usingMultiBlock) {
    qWarning("VolumeSink: the exploded view is not supported for volumes "
             "larger than the GPU's 3-D texture size limit, which are "
             "rendered in bricks.");
    emit explodedChanged(); // put the checkbox back
    return;
  }
  m_explodedEnabled = enabled;
  if (enabled && m_cutOutEnabled) {
    m_cutOutEnabled = false;
    emit cutOutChanged();
  }
  applyExploded();
  applyCutOut();
  applyScattering();
  emit lightingStateChanged();
  emit explodedChanged();
  if (refitCamera) {
    resetCameraQueued();
  }
  emit renderNeeded();
}

int VolumeSink::explodedAxis() const
{
  return m_explodedAxis;
}

void VolumeSink::setExplodedAxis(int axis, bool refitCamera)
{
  axis = qBound(0, axis, kExplodedCustomAxis);
  if (m_explodedAxis == axis) {
    return;
  }
  m_explodedAxis = axis;
  applyExploded();
  emit explodedChanged();
  if (refitCamera) {
    resetCameraQueued();
  }
  emit renderNeeded();
}

std::array<double, 3> VolumeSink::explodedDirection() const
{
  return m_explodedDirection;
}

void VolumeSink::setExplodedDirection(double x, double y, double z)
{
  const std::array<double, 3> direction = { x, y, z };
  if (direction == m_explodedDirection) {
    return;
  }
  if (x == 0.0 && y == 0.0 && z == 0.0) {
    // No direction to speak of; tell the panel so it shows the old one
    emit explodedChanged();
    return;
  }
  m_explodedDirection = direction;
  if (m_explodedAxis == kExplodedCustomAxis) {
    applyExploded();
    emit renderNeeded();
  }
  emit explodedChanged();
}

bool VolumeSink::explodedShowArrow() const
{
  return m_explodedShowArrow;
}

void VolumeSink::setExplodedShowArrow(bool show)
{
  if (m_explodedShowArrow == show) {
    return;
  }
  m_explodedShowArrow = show;
  updateExplodedWidget();
  emit explodedChanged();
  emit renderNeeded();
}

std::array<double, 3> VolumeSink::explodedUnitDirection() const
{
  return pipeline::explodedDirection(m_explodedAxis, m_explodedDirection);
}

bool VolumeSink::isExplodedSlabPlane(vtkPlane* plane) const
{
  for (const auto& pair : m_explodedSlabPlanes) {
    if (pair.first == plane || pair.second == plane) {
      return true;
    }
  }
  return false;
}

void VolumeSink::syncExplodedSlabPlanes(bool custom)
{
  auto mappers = allMappers();
  // One pair per slab. A spare pair's mapper is already gone (slabs are
  // removed before this runs), so the pair simply goes too.
  while (m_explodedSlabPlanes.size() > mappers.size()) {
    m_explodedSlabPlanes.pop_back();
  }
  while (m_explodedSlabPlanes.size() < mappers.size()) {
    m_explodedSlabPlanes.push_back({ vtkSmartPointer<vtkPlane>::New(),
                                     vtkSmartPointer<vtkPlane>::New() });
  }
  for (size_t k = 0; k < mappers.size(); ++k) {
    auto* mapper = mappers[k];
    auto* planes = mapper->GetClippingPlanes();
    for (vtkPlane* plane : { m_explodedSlabPlanes[k].first.Get(),
                             m_explodedSlabPlanes[k].second.Get() }) {
      const bool present =
        planes && planes->IndexOfFirstOccurence(plane) >= 0;
      if (custom && !present) {
        mapper->AddClippingPlane(plane);
      } else if (!custom && present) {
        mapper->RemoveClippingPlane(plane);
      }
    }
  }
}

int VolumeSink::explodedChunks() const
{
  return m_explodedChunks;
}

void VolumeSink::setExplodedChunks(int chunks)
{
  chunks = qBound(2, chunks, 16);
  if (m_explodedChunks == chunks) {
    return;
  }
  m_explodedChunks = chunks;
  applyExploded();
  emit explodedChanged();
  emit renderNeeded();
}

double VolumeSink::explodedGap() const
{
  return m_explodedGap;
}

void VolumeSink::setExplodedGap(double fraction)
{
  fraction = qBound(0.0, fraction, 1.0);
  if (m_explodedGap == fraction) {
    return;
  }
  m_explodedGap = fraction;
  applyDisplayTransform();
  emit explodedChanged();
  emit renderNeeded();
}

int VolumeSink::explodedOffset() const
{
  return m_explodedOffset;
}

void VolumeSink::setExplodedOffset(int voxels)
{
  if (m_explodedOffset == voxels) {
    return;
  }
  m_explodedOffset = voxels;
  applyExploded();
  emit explodedChanged();
  emit renderNeeded();
}

int VolumeSink::explodedOffsetLimit() const
{
  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return 0;
  }
  double bounds[6];
  vol->imageData()->GetBounds(bounds);
  const auto dir = explodedUnitDirection();
  double lo = 0.0;
  double length = 0.0;
  explodedExtent(bounds, dir, lo, length);
  return pipeline::explodedOffsetLimit(
    length, m_explodedChunks,
    explodedVoxelStep(dir, vol->imageData()->GetSpacing()));
}

void VolumeSink::teardownExplodedSlabs()
{
  // Slab 0's planes would otherwise keep cutting the whole volume
  syncExplodedSlabPlanes(/*custom=*/false);
  if (renderView()) {
    for (auto& slab : m_explodedVolumes) {
      renderView()->RemovePropFromRenderer(slab);
    }
  }
  m_explodedVolumes.clear();
  m_explodedMappers.clear();
  m_explodedSlabPlanes.clear();
  m_explodedOrderDirty = true;
}

void VolumeSink::applyExploded()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid() || !m_explodedEnabled || m_usingMultiBlock) {
    if (m_explodedEnabled && m_usingMultiBlock) {
      // Data grew past the texture limit while exploded: drop the mode
      qWarning("VolumeSink: the exploded view is not supported for volumes "
               "larger than the GPU's 3-D texture size limit, which are "
               "rendered in bricks. Turning it off.");
      m_explodedEnabled = false;
      applyScattering();
      emit lightingStateChanged();
      emit explodedChanged();
    }
    bool hadSlabs = !m_explodedVolumes.empty();
    teardownExplodedSlabs();
    if (hadSlabs) {
      double none[6] = { 0, 0, 0, 0, 0, 0 };
      m_volumeMapper->SetCroppingState(0, none, VTK_CROP_SUBVOLUME);
      applyDisplayTransform();
    }
    updateExplodedWidget();
    return;
  }

  auto* image = vol->imageData();
  const int chunks = m_explodedChunks;

  // Extra slabs are clones of slab 0's configuration on the same image.
  // Shadows stay at zero on them (see applyScattering).
  while (static_cast<int>(m_explodedMappers.size()) < chunks - 1) {
    auto mapper = vtkSmartPointer<SmartVolumeMapper>::New();
    mapper->SetScalarModeToUsePointFieldData();
    mapper->SetBlendMode(m_volumeMapper->GetBlendMode());
    mapper->SetUseJittering(m_volumeMapper->GetUseJittering());
    mapper->SetGlobalIlluminationReach(
      m_volumeMapper->GetGlobalIlluminationReach());
    mapper->SetComputeNormalFromOpacity(
      m_volumeMapper->GetComputeNormalFromOpacity());
    mapper->SetFineSampling(m_volumeMapper->GetFineSampling());
    // The user's clipping planes, not the ones bounding slab 0
    if (auto* planes = m_volumeMapper->GetClippingPlanes()) {
      planes->InitTraversal();
      while (auto* plane = planes->GetNextItem()) {
        if (!isExplodedSlabPlane(plane)) {
          mapper->AddClippingPlane(plane);
        }
      }
    }
    auto slab = vtkSmartPointer<vtkVolume>::New();
    slab->SetMapper(mapper);
    slab->SetProperty(m_volumeProperty);
    slab->SetVisibility(m_volume->GetVisibility());
    // While the view's multi-volume draws this sink, its props stay out
    // of the renderer; they go in when the sink is handed back.
    if (renderView() && !m_composited) {
      renderView()->AddPropToRenderer(slab);
    }
    m_explodedMappers.push_back(mapper);
    m_explodedVolumes.push_back(slab);
    m_explodedOrderDirty = true;
  }
  while (static_cast<int>(m_explodedMappers.size()) > chunks - 1) {
    if (renderView()) {
      renderView()->RemovePropFromRenderer(m_explodedVolumes.back());
    }
    m_explodedVolumes.pop_back();
    m_explodedMappers.pop_back();
    m_explodedOrderDirty = true;
  }

  double bounds[6];
  image->GetBounds(bounds);
  const bool custom = m_explodedAxis == kExplodedCustomAxis;
  const int axis = custom ? 0 : m_explodedAxis;
  const double lo = bounds[2 * axis];
  const double length = bounds[2 * axis + 1] - lo;
  // The cut planes all move by the offset; the volume's own faces do not
  const double shift =
    custom ? 0.0
           : explodedShift(m_explodedOffset, length, chunks,
                           image->GetSpacing()[axis]);
  auto mappers = allMappers();
  for (int k = 0; k < chunks; ++k) {
    auto* mapper = mappers[k];
    if (mapper->GetInputDataObject(0, 0) != image) {
      mapper->SetInputData(image);
    }
    if (auto* name = m_volumeMapper->GetArrayName()) {
      mapper->SelectScalarArray(name);
    }
    if (custom) {
      // Slabs at an angle are cut by a pair of clipping planes each,
      // placed with the slabs in applyDisplayTransform
      double none[6] = { 0, 0, 0, 0, 0, 0 };
      mapper->SetCroppingState(0, none, VTK_CROP_SUBVOLUME);
      continue;
    }
    double planes[6];
    for (int a = 0; a < 3; ++a) {
      planes[2 * a] = bounds[2 * a];
      planes[2 * a + 1] = bounds[2 * a + 1];
    }
    planes[2 * axis] = k == 0 ? lo : lo + shift + length * k / chunks;
    planes[2 * axis + 1] =
      k == chunks - 1 ? lo + length : lo + shift + length * (k + 1) / chunks;
    mapper->SetCroppingState(1, planes, VTK_CROP_SUBVOLUME);
  }
  syncExplodedSlabPlanes(custom);
  applyDisplayTransform();
  updateExplodedWidget();
}

void VolumeSink::applyDisplayTransform()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }
  auto pos = vol->displayPosition();
  auto orient = vol->displayOrientation();
  m_volume->SetPosition(pos.data());
  m_volume->SetOrientation(orient.data());
  if (m_explodedVolumes.empty()) {
    return;
  }

  // Each slab k sits gap * length further along the direction in data
  // space; rotate that offset the way vtkProp3D applies the orientation
  // so the slabs still line up after a display rotation.
  double bounds[6];
  vol->imageData()->GetBounds(bounds);
  const auto dir = explodedUnitDirection();
  double lo = 0.0;
  double length = 0.0;
  explodedExtent(bounds, dir, lo, length);
  vtkNew<vtkTransform> rotation;
  rotation->PostMultiply();
  rotation->RotateY(orient[1]);
  rotation->RotateX(orient[0]);
  rotation->RotateZ(orient[2]);
  const bool custom = m_explodedAxis == kExplodedCustomAxis;
  const double center[3] = { (bounds[0] + bounds[1]) / 2.0,
                             (bounds[2] + bounds[3]) / 2.0,
                             (bounds[4] + bounds[5]) / 2.0 };
  const int chunks = static_cast<int>(m_explodedVolumes.size()) + 1;
  const double shift = explodedShift(
    m_explodedOffset, length, chunks,
    explodedVoxelStep(dir, vol->imageData()->GetSpacing()));
  auto volumes = allVolumes();
  for (int k = 0; k < chunks; ++k) {
    double offset[3] = { dir[0] * k * m_explodedGap * length,
                         dir[1] * k * m_explodedGap * length,
                         dir[2] * k * m_explodedGap * length };
    rotation->TransformVector(offset, offset);
    double slabPos[3] = { pos[0] + offset[0], pos[1] + offset[1],
                          pos[2] + offset[2] };
    if (k > 0) {
      volumes[k]->SetPosition(slabPos);
      volumes[k]->SetOrientation(orient.data());
    }
    if (!custom || k >= static_cast<int>(m_explodedSlabPlanes.size())) {
      continue;
    }
    // The mappers read clipping planes in world coordinates and map
    // them back through their own prop's matrix, so each slab's pair is
    // written where that slab sits: the cut stays put in the data while
    // the slab moves.
    double normal[3] = { dir[0], dir[1], dir[2] };
    rotation->TransformVector(normal, normal);
    for (int side = 0; side < 2; ++side) {
      // Every cut plane moves by the offset; the volume's own two faces
      // (below slab 0, above the last) stay where they are
      const bool face = (k == 0 && side == 0) || (k == chunks - 1 && side == 1);
      const double d =
        lo + length * (k + side) / chunks + (face ? 0.0 : shift);
      double origin[3] = { center[0] + dir[0] * d, center[1] + dir[1] * d,
                           center[2] + dir[2] * d };
      rotation->TransformVector(origin, origin);
      auto& plane = side == 0 ? m_explodedSlabPlanes[k].first
                              : m_explodedSlabPlanes[k].second;
      plane->SetOrigin(origin[0] + slabPos[0], origin[1] + slabPos[1],
                       origin[2] + slabPos[2]);
      // Lower plane keeps what is beyond it; upper keeps what is before
      const double sign = side == 0 ? 1.0 : -1.0;
      plane->SetNormal(sign * normal[0], sign * normal[1],
                       sign * normal[2]);
    }
  }
}

void VolumeSink::sortExplodedProps()
{
  // Nothing of this sink's is in the renderer to sort while the view's
  // multi-volume draws it, and AddItem below would put the slabs there.
  if (m_explodedVolumes.empty() || !renderView() || m_composited) {
    return;
  }
  auto* renderer = renderView()->GetRenderer();
  auto* camera = renderer ? renderer->GetActiveCamera() : nullptr;
  if (!camera) {
    return;
  }
  // Slab k lies further along the axis the larger k is. With the camera
  // looking along +axis the high slabs are the far ones and must draw
  // first; otherwise slab 0 is farthest.
  const auto dir = explodedUnitDirection();
  double axisDir[3] = { dir[0], dir[1], dir[2] };
  auto vol = volumeData();
  auto orient = vol && vol->isValid() ? vol->displayOrientation()
                                      : std::array<double, 3>{ 0, 0, 0 };
  vtkNew<vtkTransform> rotation;
  rotation->PostMultiply();
  rotation->RotateY(orient[1]);
  rotation->RotateX(orient[0]);
  rotation->RotateZ(orient[2]);
  rotation->TransformVector(axisDir, axisDir);
  double view[3];
  camera->GetDirectionOfProjection(view);
  bool reversed = view[0] * axisDir[0] + view[1] * axisDir[1] +
                    view[2] * axisDir[2] > 0.0;
  if (!m_explodedOrderDirty && reversed == m_explodedOrderReversed) {
    return;
  }
  m_explodedOrderReversed = reversed;
  m_explodedOrderDirty = false;
  auto volumes = allVolumes();
  if (reversed) {
    std::reverse(volumes.begin(), volumes.end());
  }
  // Reorder the collection itself: RemoveViewProp would release each
  // slab's GPU texture and force a full re-upload on the next frame.
  auto* props = renderer->GetViewProps();
  for (auto* slab : volumes) {
    props->RemoveItem(slab);
    props->AddItem(slab);
  }
}

void VolumeSink::resetCameraQueued()
{
  vtkWeakPointer<vtkSMRenderViewProxy> proxy(
    vtkSMRenderViewProxy::SafeDownCast(view()));
  if (proxy) {
    QMetaObject::invokeMethod(
      this,
      [proxy]() {
        if (proxy) {
          proxy->ResetCamera();
        }
      },
      Qt::QueuedConnection);
  }
}

void VolumeSink::applyCutOut()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  if (m_explodedEnabled) {
    return; // applyExploded() owns the cropping planes
  }
  if (!m_cutOutEnabled) {
    double none[6] = { 0, 0, 0, 0, 0, 0 };
    m_volumeMapper->SetCroppingState(0, none, VTK_CROP_SUBVOLUME);
    return;
  }

  double bounds[6];
  vol->imageData()->GetBounds(bounds);

  // Two planes per axis give VTK's 27 crop regions; the second plane
  // at the far edge collapses the third region per axis.
  double planes[6];
  for (int axis = 0; axis < 3; ++axis) {
    double lo = bounds[2 * axis];
    double hi = bounds[2 * axis + 1];
    planes[2 * axis] = lo + m_cutOutPosition[axis] * (hi - lo);
    planes[2 * axis + 1] = hi;
  }

  // Region bits run x fastest, then y, then z; drop the octant on the
  // high side of every axis whose corner bit is set.
  int i = (m_cutOutCorner & 1) ? 1 : 0;
  int j = (m_cutOutCorner & 2) ? 1 : 0;
  int k = (m_cutOutCorner & 4) ? 1 : 0;
  int flags = 0x7ffffff & ~(1 << (i + 3 * j + 9 * k));

  m_volumeMapper->SetCroppingState(1, planes, flags);

  if (m_usingMultiBlock) {
    qWarning("VolumeSink: cut-out rendering is not supported for volumes "
             "larger than the GPU's 3-D texture size limit. This volume is "
             "rendered in bricks, so the cut-out will have no effect.");
  }
}

// --- Multi-volume ---

bool VolumeSink::multiVolumeActive() const
{
  return m_composited;
}

bool VolumeSink::multiVolumeLead() const
{
  return m_composited && m_leadApplied;
}

vtkVolume* VolumeSink::volumeProp() const
{
  return m_volume;
}

vtkImageData* VolumeSink::renderedImage() const
{
  return vtkImageData::SafeDownCast(m_volumeMapper->GetDataObjectInput());
}

QString VolumeSink::renderedArrayName() const
{
  const char* name = m_volumeMapper->GetArrayName();
  return name ? QString::fromUtf8(name) : QString();
}

bool VolumeSink::multiVolumeEligible() const
{
  // The prop's own flag is the one source of truth for "on screen": it
  // folds in the sink's visibility, whether there is data, and whether a
  // subclass draws the data some other way (LabelMapSink's surface).
  // Bricked volumes are out: a vtkMultiVolume takes one texture per input.
  return renderView() && m_volume->GetVisibility() != 0 &&
         !m_usingMultiBlock && renderedImage() != nullptr;
}

void VolumeSink::syncMultiVolumeMembership()
{
  auto* viewProxy = view();
  if (!viewProxy) {
    return;
  }
  if (multiVolumeEligible()) {
    if (auto* coordinator = MultiVolumeCoordinator::forView(viewProxy)) {
      coordinator->addMember(this);
    }
  } else if (auto* coordinator = MultiVolumeCoordinator::find(viewProxy)) {
    coordinator->removeMember(this);
  }
}

void VolumeSink::showStandaloneProps(bool shown)
{
  if (!renderView()) {
    return;
  }
  if (shown) {
    renderView()->AddPropToRenderer(m_volume);
    for (auto& slab : m_explodedVolumes) {
      renderView()->AddPropToRenderer(slab);
    }
    m_explodedOrderDirty = true;
  } else {
    renderView()->RemovePropFromRenderer(m_volume);
    for (auto& slab : m_explodedVolumes) {
      renderView()->RemovePropFromRenderer(slab);
    }
  }
}

void VolumeSink::applyMultiVolumeState()
{
  auto* coordinator = MultiVolumeCoordinator::find(view());
  const bool composited =
    coordinator && coordinator->active() && coordinator->isMember(this);
  const bool lead = composited && coordinator->lead() == this;
  bool changed = false;
  if (composited != m_composited) {
    m_composited = composited;
    showStandaloneProps(!composited);
    changed = true;
  }
  if (lead != m_leadApplied) {
    m_leadApplied = lead;
    changed = true;
  }
  if (changed) {
    updateExplodedWidget();
    emit multiVolumeStateChanged();
    // Scattering availability is part of the lighting state shown.
    emit lightingStateChanged();
    emit renderNeeded();
  }
}

// --- Properties widget ---

bool VolumeSink::scatteringAvailable() const
{
  return scatteringSupported() && !m_composited;
}

QString VolumeSink::scatteringUnavailableReason() const
{
  if (scatteringAvailable()) {
    return QString();
  }
  if (m_composited) {
    return tr("Volumetric shadows are unavailable while the volumes in this "
              "view are rendered together, which VTK cannot shadow. They "
              "come back once only one volume is shown.");
  }
  if (m_usingMultiBlock) {
    return tr("This volume is larger than the GPU's 3-D texture size limit, "
              "so it is rendered in bricks. Volumetric shadows cannot be "
              "bounded on that path and are unavailable here. Subsample or "
              "crop the volume to use them.");
  }
  if (m_explodedEnabled) {
    return tr("Volumetric shadows are unavailable while the exploded view "
              "is on, since every slab would render its own shadow pass.");
  }
  return tr("Volumetric shadows are turned off by the "
            "Volume.AllowVolumetricScattering setting.");
}

bool VolumeSink::confirmVolumetricShadows(QWidget* parent) const
{
  auto* core = pqApplicationCore::instance();
  auto* settings = core ? core->settings() : nullptr;
  const QString key = "Volume.WarnVolumetricScattering";
  if (settings && !settings->value(key, true).toBool()) {
    return true;
  }

  // Worth interrupting for: when this goes wrong it is not a slow render but
  // a lost session. A hung GPU command buffer is unrecoverable on macOS - the
  // driver aborts the process - and the GPU reset that follows can take other
  // applications down with it.
  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(tr("Volumetric Shadows"));
  box.setText(tr("This turns on volumetric shadows."));
  box.setInformativeText(
    tr("They are far more demanding than the rest of the lighting. Tomviz "
       "renders them at reduced resolution to stay within what the GPU can "
       "finish, "
       "but on lower-end hardware a frame can still take long enough for the "
       "driver to reset the GPU, which will close Tomviz - and possibly other "
       "applications - without warning.\n\n"
       "Save your work before continuing."));
  auto* proceed = box.addButton(tr("Continue"), QMessageBox::AcceptRole);
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(proceed);

  // QMessageBox takes ownership of the check box.
  auto* dontAsk = new QCheckBox(tr("Don't warn me again"));
  box.setCheckBox(dontAsk);
  box.exec();

  const bool accepted = box.clickedButton() == proceed;
  if (accepted && dontAsk->isChecked() && settings) {
    settings->setValue(key, false);
  }
  return accepted;
}

QWidget* VolumeSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new VolumeSinkWidget(parent);
  int insertRow = 0;

  // --- Active Scalars combo (row 0) ---
  m_scalarsCombo = new QComboBox(widget);
  populateScalarsCombo();
  widget->formLayout()->insertRow(insertRow++, "Active Scalars", m_scalarsCombo);
  connect(m_scalarsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            setActiveScalars(m_scalarsCombo->itemData(idx).toInt());
          });

  // --- Separate Color Map checkbox ---
  auto* separateCmapCheck = new QCheckBox(widget);
  {
    QSignalBlocker blocker(separateCmapCheck);
    separateCmapCheck->setChecked(useDetachedColorMap());
  }
  widget->formLayout()->insertRow(insertRow++, "Separate Color Map",
                                  separateCmapCheck);
  connect(separateCmapCheck, &QCheckBox::toggled,
          [this](bool on) { setUseDetachedColorMap(on); });

  // --- Cut-out ---
  // A checkable group below Lighting; the corner and position rows are
  // only shown while the cut-out is on so the panel stays uncluttered.
  // An unchecked group collapses to its title row so nothing empty is
  // drawn below it.
  auto collapseUnless = [](CollapsingGroupBox* box, QWidget* body, bool on) {
    body->setVisible(on);
    box->setCollapsed(!on);
  };

  auto* cutOutBox = new CollapsingGroupBox("Cut Out", widget);
  cutOutBox->setCheckable(true);
  cutOutBox->setToolTip(
    "Remove one octant of the volume so the interior can be seen from "
    "outside.");
  {
    QSignalBlocker blocker(cutOutBox);
    cutOutBox->setChecked(cutOutEnabled());
  }
  auto* cutOutBody = new QWidget(cutOutBox);
  auto* cutOutForm = new QFormLayout(cutOutBody);
  cutOutForm->setContentsMargins(0, 0, 0, 0);
  auto* cutOutLayout = new QVBoxLayout(cutOutBox);
  cutOutLayout->addWidget(cutOutBody);
  connect(cutOutBox, &QGroupBox::toggled, this,
          [this](bool on) { setCutOutEnabled(on); });

  auto* cornerCombo = new QComboBox(cutOutBody);
  // Order matches the corner bits: 1 = high X, 2 = high Y, 4 = high Z.
  cornerCombo->addItems({ "-X -Y -Z", "+X -Y -Z", "-X +Y -Z", "+X +Y -Z",
                          "-X -Y +Z", "+X -Y +Z", "-X +Y +Z", "+X +Y +Z" });
  cornerCombo->setToolTip("Which corner of the volume is removed.");
  {
    QSignalBlocker blocker(cornerCombo);
    cornerCombo->setCurrentIndex(cutOutCorner());
  }
  cutOutForm->addRow("Corner", cornerCombo);
  connect(cornerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) { setCutOutCorner(idx); });

  const char* axisNames[3] = { "X", "Y", "Z" };
  for (int axis = 0; axis < 3; ++axis) {
    auto* slider = new DoubleSliderWidget(true, cutOutBody);
    slider->setLineEditWidth(50);
    slider->setMinimum(0.0);
    slider->setMaximum(1.0);
    slider->setValue(cutOutPosition(axis));
    slider->setToolTip(
      "Where the cut sits along this axis, as a fraction of the volume.");
    cutOutForm->addRow(axisNames[axis], slider);
    connect(slider, &DoubleSliderWidget::valueEdited, this,
            [this, axis](double v) { setCutOutPosition(axis, v); });
    connect(slider, &DoubleSliderWidget::valueChanged, this,
            [this, axis](double v) { setCutOutPosition(axis, v); });
  }

  auto syncCutOut = [this, cutOutBox, cutOutBody, collapseUnless]() {
    QSignalBlocker blocker(cutOutBox);
    cutOutBox->setChecked(cutOutEnabled());
    collapseUnless(cutOutBox, cutOutBody, cutOutEnabled());
  };
  syncCutOut();
  connect(this, &VolumeSink::cutOutChanged, widget,
          [syncCutOut]() { syncCutOut(); });

  // --- Exploded view ---
  auto* explodedBox = new CollapsingGroupBox("Exploded View", widget);
  explodedBox->setCheckable(true);
  explodedBox->setToolTip(
    "Render the volume as slabs pulled apart along one axis. The data is "
    "not modified.");
  {
    QSignalBlocker blocker(explodedBox);
    explodedBox->setChecked(explodedEnabled());
  }
  auto* explodedBody = new QWidget(explodedBox);
  auto* explodedForm = new QFormLayout(explodedBody);
  explodedForm->setContentsMargins(0, 0, 0, 0);
  auto* explodedLayout = new QVBoxLayout(explodedBox);
  explodedLayout->addWidget(explodedBody);
  connect(explodedBox, &QGroupBox::toggled, this,
          [this](bool on) { setExplodedEnabled(on); });

  auto* axisCombo = new QComboBox(explodedBody);
  axisCombo->addItems({ "X", "Y", "Z", "Custom" });
  {
    QSignalBlocker blocker(axisCombo);
    axisCombo->setCurrentIndex(explodedAxis());
  }
  explodedForm->addRow("Axis", axisCombo);
  connect(axisCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) { setExplodedAxis(idx); });

  // A custom direction: three components, normalized when used, and the
  // arrow that edits it by dragging. Both rows only exist while the axis
  // is Custom.
  auto* directionRow = new QWidget(explodedBody);
  auto* directionLayout = new QHBoxLayout(directionRow);
  directionLayout->setContentsMargins(0, 0, 0, 0);
  std::array<QDoubleSpinBox*, 3> directionSpins;
  for (int i = 0; i < 3; ++i) {
    auto* spin = new QDoubleSpinBox(directionRow);
    spin->setRange(-1000.0, 1000.0);
    spin->setDecimals(3);
    spin->setSingleStep(0.1);
    // Commit on Enter or focus loss: every change re-cuts the slabs
    spin->setKeyboardTracking(false);
    spin->setToolTip(QString("%1 component of the direction the slabs are "
                             "cut and pulled apart along, in data "
                             "coordinates. The length does not matter.")
                       .arg(QChar('X' + i)));
    directionLayout->addWidget(spin);
    directionSpins[i] = spin;
  }
  const int directionRowIndex = explodedForm->rowCount();
  explodedForm->addRow("Direction", directionRow);
  auto readDirection = [this, directionSpins]() {
    setExplodedDirection(directionSpins[0]->value(),
                         directionSpins[1]->value(),
                         directionSpins[2]->value());
  };
  for (auto* spin : directionSpins) {
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [readDirection](double) { readDirection(); });
  }
  auto* arrowCheck = new QCheckBox("Show Arrow", explodedBody);
  arrowCheck->setToolTip(
    "Draw an arrow along the direction at the center of the volume; drag "
    "its tip to turn it.");
  const int arrowRowIndex = explodedForm->rowCount();
  explodedForm->addRow(QString(), arrowCheck);
  connect(arrowCheck, &QCheckBox::toggled, this,
          [this](bool on) { setExplodedShowArrow(on); });

  auto* chunksSpin = new QSpinBox(explodedBody);
  chunksSpin->setRange(2, 16);
  chunksSpin->setValue(explodedChunks());
  chunksSpin->setToolTip("How many slabs the volume is split into.");
  explodedForm->addRow("Slabs", chunksSpin);
  connect(chunksSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int n) { setExplodedChunks(n); });

  auto* gapSlider = new DoubleSliderWidget(true, explodedBody);
  gapSlider->setLineEditWidth(50);
  gapSlider->setMinimum(0.0);
  gapSlider->setMaximum(1.0);
  gapSlider->setValue(explodedGap());
  gapSlider->setToolTip(
    "Space between slabs, as a fraction of the volume's length along the "
    "axis.");
  explodedForm->addRow("Gap", gapSlider);
  connect(gapSlider, &DoubleSliderWidget::valueEdited, this,
          [this](double v) { setExplodedGap(v); });
  connect(gapSlider, &DoubleSliderWidget::valueChanged, this,
          [this](double v) { setExplodedGap(v); });

  auto* offsetSpin = new QSpinBox(explodedBody);
  offsetSpin->setSuffix(" voxels");
  offsetSpin->setKeyboardTracking(false);
  offsetSpin->setToolTip(
    "Slide every cut along the direction by this many voxels, to put the "
    "gaps where you want them. The first slab grows by the offset and the "
    "last shrinks (or the other way round for a negative offset); the "
    "range keeps every slab at least one voxel thick.");
  explodedForm->addRow("Offset", offsetSpin);
  connect(offsetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int v) { setExplodedOffset(v); });

  auto syncExploded = [this, explodedBox, explodedBody, explodedForm,
                       axisCombo, directionSpins, directionRowIndex,
                       arrowCheck, arrowRowIndex, chunksSpin, gapSlider,
                       offsetSpin, collapseUnless]() {
    QSignalBlocker b1(explodedBox), b2(axisCombo), b3(chunksSpin),
      b4(gapSlider), b5(arrowCheck), b6(offsetSpin);
    explodedBox->setChecked(explodedEnabled());
    collapseUnless(explodedBox, explodedBody, explodedEnabled());
    axisCombo->setCurrentIndex(explodedAxis());
    const bool custom = explodedAxis() == kExplodedCustomAxis;
    explodedForm->setRowVisible(directionRowIndex, custom);
    explodedForm->setRowVisible(arrowRowIndex, custom);
    const auto direction = explodedDirection();
    for (int i = 0; i < 3; ++i) {
      QSignalBlocker blocker(directionSpins[i]);
      directionSpins[i]->setValue(direction[i]);
    }
    arrowCheck->setChecked(explodedShowArrow());
    chunksSpin->setValue(explodedChunks());
    gapSlider->setValue(explodedGap());
    // The bound follows the data and the slab count; the stored value
    // is shown clamped, as it is used
    const int limit = explodedOffsetLimit();
    offsetSpin->setRange(-limit, limit);
    offsetSpin->setValue(qBound(-limit, explodedOffset(), limit));
  };
  syncExploded();
  connect(this, &VolumeSink::explodedChanged, widget,
          [syncExploded]() { syncExploded(); });

  // Below the Lighting group, ahead of the trailing stretch
  auto* mainLayout = widget->mainLayout();
  mainLayout->insertWidget(mainLayout->count() - 1, cutOutBox);
  mainLayout->insertWidget(mainLayout->count() - 1, explodedBox);

  // Push all lighting state (values + active preset highlight) into the
  // widget; reused whenever any lighting parameter changes on this sink.
  auto syncLighting = [this, widget]() {
    QSignalBlocker blocker(widget);
    widget->setLighting(lighting());
    widget->setAmbient(ambient());
    widget->setDiffuse(diffuse());
    widget->setSpecular(specular());
    widget->setSpecularPower(specularPower());
    widget->setVolumetricScattering(volumetricScattering());
    widget->setShadowsEnabled(shadowsEnabled());
    widget->setScatteringOverBudget(scatteringOverBudget());
    widget->setShadowReach(shadowReach());
    widget->setAnisotropy(scatteringAnisotropy());
    widget->setSmoothNormals(smoothNormals());
    widget->setActiveLightingPreset(static_cast<int>(currentLightingPreset()));
    widget->setActiveUserLightingPreset(matchingUserLightingPreset());
    widget->setScatteringAvailable(scatteringAvailable(),
                                   scatteringUnavailableReason());
  };
  connect(this, &VolumeSink::lightingStateChanged, widget, syncLighting);

  // While the view's multi-volume draws this sink, show what actually
  // renders (composite, jittered) in the controls it takes away, and grey
  // out what has no effect on that path. The sink's own settings are kept
  // underneath and come back when it renders on its own again.
  const QString cutOutToolTip = cutOutBox->toolTip();
  const QString explodedToolTip = explodedBox->toolTip();
  auto syncMultiVolume = [this, widget, cutOutBox, explodedBox, cutOutToolTip,
                          explodedToolTip]() {
    const bool active = multiVolumeActive();
    const bool lead = multiVolumeLead();
    QString leadLabel;
    if (active && !lead) {
      auto* coordinator = MultiVolumeCoordinator::find(view());
      if (coordinator && coordinator->lead()) {
        leadLabel = coordinator->lead()->label();
      }
    }
    {
      QSignalBlocker blocker(widget);
      widget->setBlendingMode(active ? vtkVolumeMapper::COMPOSITE_BLEND
                                     : blendingMode());
      widget->setJittering(active ? true : jittering());
    }
    widget->setMultiVolumeMode(active, lead, leadLabel);
    const QString reason =
      tr("Not available while the volumes in this view are rendered "
         "together, which VTK cannot crop.");
    cutOutBox->setEnabled(!active);
    cutOutBox->setToolTip(active ? reason : cutOutToolTip);
    explodedBox->setEnabled(!active);
    explodedBox->setToolTip(active ? reason : explodedToolTip);
  };
  connect(this, &VolumeSink::multiVolumeStateChanged, widget,
          syncMultiVolume);

  // Push current state into the widget
  {
    QSignalBlocker blocker(widget);
    widget->setJittering(jittering());
    widget->setBlendingMode(blendingMode());
    widget->setInterpolationType(interpolationType());
    widget->setSolidity(solidity());
  }
  syncLighting();
  syncMultiVolume();

  // Connect widget signals to VolumeSink setters
  connect(widget, &VolumeSinkWidget::jitteringToggled, this,
          &VolumeSink::setJittering);
  connect(widget, &VolumeSinkWidget::lightingToggled, this,
          &VolumeSink::setLighting);
  connect(widget, &VolumeSinkWidget::blendingChanged, this,
          &VolumeSink::setBlendingMode);
  connect(widget, &VolumeSinkWidget::interpolationChanged, this,
          &VolumeSink::setInterpolationType);
  connect(this, &VolumeSink::interpolationTypeChanged, widget,
          [widget](int type) {
            QSignalBlocker blocker(widget);
            widget->setInterpolationType(type);
          });
  connect(widget, &VolumeSinkWidget::ambientChanged, this,
          &VolumeSink::setAmbient);
  connect(widget, &VolumeSinkWidget::diffuseChanged, this,
          &VolumeSink::setDiffuse);
  connect(widget, &VolumeSinkWidget::specularChanged, this,
          &VolumeSink::setSpecular);
  connect(widget, &VolumeSinkWidget::specularPowerChanged, this,
          &VolumeSink::setSpecularPower);
  connect(widget, &VolumeSinkWidget::volumetricScatteringChanged, this,
          [this, widget](double value) {
            // Raising the strength from zero is the third way to switch
            // shadows on, so it gets the same warning as the presets and
            // the Shadows box. Only when the box would let them render.
            if (value > 0.0 && shadowsEnabled() &&
                effectiveScattering() <= 0.0 &&
                !confirmVolumetricShadows(widget)) {
              QSignalBlocker blocker(widget);
              widget->setVolumetricScattering(volumetricScattering());
              return;
            }
            setVolumetricScattering(value);
          });
  connect(widget, &VolumeSinkWidget::shadowReachChanged, this,
          &VolumeSink::setShadowReach);
  connect(widget, &VolumeSinkWidget::anisotropyChanged, this,
          &VolumeSink::setScatteringAnisotropy);
  connect(widget, &VolumeSinkWidget::smoothNormalsToggled, this,
          &VolumeSink::setSmoothNormals);
  connect(widget, &VolumeSinkWidget::lightingPresetClicked, this,
          [this, widget](int preset) {
            auto p = static_cast<LightingPreset>(preset);
            int idx = static_cast<int>(p);
            const bool castsShadows = idx >= 0 &&
                                      idx < kNumLightingPresets &&
                                      kLightingPresets[idx].scattering > 0.0;
            if (castsShadows && !confirmVolumetricShadows(widget)) {
              // Put the highlight back on whatever is actually rendering.
              QSignalBlocker blocker(widget);
              widget->setActiveLightingPreset(
                static_cast<int>(currentLightingPreset()));
              return;
            }
            applyLightingPreset(p);
          });
  connect(widget, &VolumeSinkWidget::userLightingPresetSelected, this,
          [this, widget](const QString& name) {
            auto preset = LightingPresetStore::instance().preset(name);
            if (preset.name.isEmpty()) {
              return;
            }
            if (preset.scattering > 0.0 && !confirmVolumetricShadows(widget)) {
              QSignalBlocker blocker(widget);
              widget->setActiveUserLightingPreset(
                matchingUserLightingPreset());
              return;
            }
            applyUserLightingPreset(preset);
          });
  connect(widget, &VolumeSinkWidget::saveUserLightingPresetRequested, this,
          [this, widget]() {
            auto& store = LightingPresetStore::instance();
            QString suggested;
            for (int n = 1;; ++n) {
              suggested = QString("Lighting %1").arg(n);
              if (!store.contains(suggested)) {
                break;
              }
            }
            bool ok = false;
            auto name = QInputDialog::getText(widget, "Save Lighting Preset",
                                              "Preset name:", QLineEdit::Normal,
                                              suggested, &ok)
                          .trimmed();
            if (!ok || name.isEmpty()) {
              return;
            }
            if (store.contains(name) &&
                QMessageBox::question(widget, "Save Lighting Preset",
                                      QString("Replace the saved preset "
                                              "\"%1\"?")
                                        .arg(name)) != QMessageBox::Yes) {
              return;
            }
            auto preset = currentLightingValues();
            preset.name = name;
            store.save(preset);
            widget->setActiveUserLightingPreset(name);
          });
  connect(widget, &VolumeSinkWidget::renameUserLightingPresetRequested, this,
          [widget](const QString& name) {
            auto& store = LightingPresetStore::instance();
            bool ok = false;
            auto newName =
              QInputDialog::getText(widget, "Rename Lighting Preset",
                                    "Preset name:", QLineEdit::Normal, name,
                                    &ok)
                .trimmed();
            if (!ok || newName.isEmpty() || newName == name) {
              return;
            }
            if (store.contains(newName)) {
              QMessageBox::warning(widget, "Rename Lighting Preset",
                                   QString("A saved preset called \"%1\" "
                                           "already exists.")
                                     .arg(newName));
              return;
            }
            if (store.rename(name, newName)) {
              widget->setActiveUserLightingPreset(newName);
            }
          });
  connect(widget, &VolumeSinkWidget::deleteUserLightingPresetRequested, this,
          [](const QString& name) {
            LightingPresetStore::instance().remove(name);
          });
  connect(widget, &VolumeSinkWidget::shadowsToggled, this,
          [this, widget](bool enabled) {
            // Only switching them on is worth a warning, and only when there
            // is a scattering level for it to restore.
            if (enabled && volumetricScattering() > 0.0 &&
                !confirmVolumetricShadows(widget)) {
              QSignalBlocker blocker(widget);
              widget->setShadowsEnabled(shadowsEnabled());
              return;
            }
            setShadowsEnabled(enabled);
          });
  connect(widget, &VolumeSinkWidget::solidityChanged, this,
          &VolumeSink::setSolidity);
  // Animations and Go To set the solidity too; keep the slider on it
  connect(this, &VolumeSink::solidityChanged, widget, [widget](double value) {
    QSignalBlocker blocker(widget);
    widget->setSolidity(value);
  });

  return widget;
}

// --- Serialization ---

QJsonObject VolumeSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["interpolation"] = interpolationType();
  json["blendingMode"] = blendingMode();
  json["rayJittering"] = jittering();
  json["solidity"] = solidity();
  json["activeScalars"] = activeScalarsToName(m_activeScalars);
  json["labelMapDefaultsApplied"] = m_labelMapDefaultsApplied;

  QJsonObject cutOut;
  cutOut["enabled"] = m_cutOutEnabled;
  cutOut["corner"] = m_cutOutCorner;
  cutOut["position"] = QJsonArray{ m_cutOutPosition[0], m_cutOutPosition[1],
                                   m_cutOutPosition[2] };
  json["cutOut"] = cutOut;

  QJsonObject exploded;
  exploded["enabled"] = m_explodedEnabled;
  exploded["axis"] = m_explodedAxis;
  exploded["direction"] = QJsonArray{ m_explodedDirection[0],
                                      m_explodedDirection[1],
                                      m_explodedDirection[2] };
  exploded["showArrow"] = m_explodedShowArrow;
  exploded["chunks"] = m_explodedChunks;
  exploded["gap"] = m_explodedGap;
  exploded["offset"] = m_explodedOffset;
  json["exploded"] = exploded;

  QJsonObject light;
  light["enabled"] = lighting();
  light["ambient"] = ambient();
  light["diffuse"] = diffuse();
  light["specular"] = specular();
  light["specularPower"] = specularPower();
  light["scattering"] = volumetricScattering();
  light["shadowsEnabled"] = shadowsEnabled();
  light["shadowReach"] = shadowReach();
  light["anisotropy"] = scatteringAnisotropy();
  light["smoothNormals"] = smoothNormals();
  json["lighting"] = light;

  return json;
}

bool VolumeSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("interpolation")) {
    setInterpolationType(json["interpolation"].toInt());
  }
  if (json.contains("blendingMode")) {
    setBlendingMode(json["blendingMode"].toInt());
  }
  if (json.contains("cutOut")) {
    auto cutOut = json["cutOut"].toObject();
    m_cutOutCorner = qBound(0, cutOut["corner"].toInt(), 7);
    auto position = cutOut["position"].toArray();
    for (int axis = 0; axis < 3 && axis < position.size(); ++axis) {
      m_cutOutPosition[axis] = qBound(0.0, position[axis].toDouble(), 1.0);
    }
    m_cutOutEnabled = cutOut["enabled"].toBool();
    applyCutOut();
  }
  if (json.contains("exploded")) {
    auto exploded = json["exploded"].toObject();
    m_explodedAxis =
      qBound(0, exploded["axis"].toInt(2), kExplodedCustomAxis);
    auto direction = exploded["direction"].toArray();
    if (direction.size() == 3) {
      m_explodedDirection = { direction[0].toDouble(),
                              direction[1].toDouble(),
                              direction[2].toDouble() };
    }
    m_explodedShowArrow = exploded["showArrow"].toBool(true);
    m_explodedChunks = qBound(2, exploded["chunks"].toInt(4), 16);
    m_explodedGap = qBound(0.0, exploded["gap"].toDouble(0.25), 1.0);
    m_explodedOffset = exploded["offset"].toInt(0);
    m_explodedEnabled = exploded["enabled"].toBool();
    if (m_explodedEnabled) {
      m_cutOutEnabled = false;
    }
    applyExploded();
  }
  if (json.contains("rayJittering")) {
    setJittering(json["rayJittering"].toBool());
  }
  if (json.contains("solidity")) {
    setSolidity(json["solidity"].toDouble());
  }
  if (json.contains("lighting")) {
    auto light = json["lighting"].toObject();
    setLighting(light["enabled"].toBool());
    setAmbient(light["ambient"].toDouble());
    setDiffuse(light["diffuse"].toDouble());
    setSpecular(light["specular"].toDouble());
    setSpecularPower(light["specularPower"].toDouble());
    // Missing in pre-preset state files; the defaults match their behavior.
    setVolumetricScattering(light["scattering"].toDouble(0.0));
    // Missing before the shadow switch existed, where a stored scattering
    // level was always rendered.
    setShadowsEnabled(light["shadowsEnabled"].toBool(true));
    setShadowReach(light["shadowReach"].toDouble(0.0));
    setScatteringAnisotropy(light["anisotropy"].toDouble(0.0));
    setSmoothNormals(light["smoothNormals"].toBool(false));
  }
  if (json.contains("activeScalars")) {
    readActiveScalars(json, m_activeScalars);
  }
  // Pre-feature state files won't have the key; assume their saved
  // settings are intentional and skip the auto-override.
  m_labelMapDefaultsApplied =
    json["labelMapDefaultsApplied"].toBool(true);
  return true;
}

void VolumeSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol) return;
  applyDisplayTransform();
  applyActiveScalars();
  QMetaObject::invokeMethod(this, &VolumeSink::populateScalarsCombo,
                            Qt::QueuedConnection);
  emit renderNeeded();
}

void VolumeSink::populateScalarsCombo()
{
  if (!m_scalarsCombo) {
    return;
  }

  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  QSignalBlocker blocker(m_scalarsCombo);

  m_scalarsCombo->clear();
  m_scalarsCombo->addItem("Default", -1);

  auto* pointData = vol->imageData()->GetPointData();
  for (int i = 0; i < pointData->GetNumberOfArrays(); ++i) {
    auto* array = pointData->GetArray(i);
    if (array && array->GetName()) {
      m_scalarsCombo->addItem(QString(array->GetName()), i);
    }
  }

  if (m_activeScalars < 0) {
    m_scalarsCombo->setCurrentIndex(0);
  } else {
    int idx = m_scalarsCombo->findData(m_activeScalars);
    m_scalarsCombo->setCurrentIndex(idx >= 0 ? idx : 0);
  }
}

} // namespace pipeline
} // namespace tomviz
