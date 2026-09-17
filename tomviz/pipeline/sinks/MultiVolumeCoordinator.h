/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineMultiVolumeCoordinator_h
#define tomvizPipelineMultiVolumeCoordinator_h

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include <array>

class vtkCallbackCommand;
class vtkGPUVolumeRayCastMapper;
class vtkImageData;
class vtkLight;
class vtkMultiVolume;
class vtkPVRenderView;
class vtkSMViewProxy;

namespace tomviz {
namespace pipeline {

class MultiVolumeMapper;
class VolumeSink;

/// Renders the volumes of one view together once there are two of them.
///
/// A vtkVolume composites into the frame on its own, so where two of them
/// overlap the second is drawn over a finished picture of the first and the
/// overlap comes out wrong, in an order that depends on which prop the
/// renderer reached first. VTK's answer is vtkMultiVolume: one
/// vtkGPUVolumeRayCastMapper with an input port per volume, marching every
/// volume along the same ray. There is one coordinator per view; the view's
/// VolumeSinks join it whenever they have a volume on screen, and from the
/// second member on the coordinator takes their props out of the renderer
/// and draws them through the multi-volume instead. With one member left
/// it hands the props back, so a lone volume renders exactly as before.
///
/// What VTK's multi-volume path can and cannot do decides what a member
/// keeps. Color, opacity, interpolation and solidity stay per volume.
/// Everything mapper-level is shared: composite blending, ray jittering,
/// clipping planes (gathered from every member), sampling. Shading is
/// decided for the whole set by the property at port 0, which belongs to
/// the lead member, the earliest one to join. Cropping and volumetric
/// shadows are not available on this path at all; the sinks grey those
/// controls out while they are members.
///
/// One more thing the path cannot do is shade under anything but a single
/// headlight: with any other lights in the renderer its shader drops the
/// lighting altogether. ParaView views light the scene with a five-light
/// kit, so while the multi-volume is active the coordinator takes the kit
/// out of the renderer and puts a headlight in, and reverses that when it
/// deactivates. Surfaces in the view are lit by that headlight meanwhile.
///
/// The mapper is also replaced, not reused, after its GPU resources have
/// been released (which removing the prop from the renderer does, as does
/// a recreated GL context). VTK clears the mapper's "needs initialization"
/// flag only on its single-input path, so a released multi-input mapper
/// would otherwise re-upload every volume and rebuild its shader on every
/// frame from then on.
///
/// Each member's input reaches the mapper as a shallow copy whose active
/// scalars are the array the sink selected, because the shared mapper has
/// one array selection for all its ports.
class MultiVolumeCoordinator : public QObject
{
  Q_OBJECT

public:
  /// The coordinator for @a view, created on first use.
  static MultiVolumeCoordinator* forView(vtkSMViewProxy* view);
  /// The coordinator for @a view, or null when it has none.
  static MultiVolumeCoordinator* find(vtkSMViewProxy* view);

  /// From this many members on, the view renders through the multi-volume.
  static constexpr int kMinimumMembers = 2;
  /// vtkGPUVolumeRayCastMapper has this many input ports.
  static constexpr int kMaximumMembers = 10;

  ~MultiVolumeCoordinator() override;

  /// Bring @a sink into the set; a sink already in it has its input
  /// refreshed instead. Returns false when the set is full, in which case
  /// the sink keeps rendering on its own.
  bool addMember(VolumeSink* sink);
  /// Take @a sink out of the set. The coordinator deletes itself once the
  /// last member is gone.
  void removeMember(VolumeSink* sink);
  bool isMember(const VolumeSink* sink) const;

  /// The member's data or scalar selection may have changed: hand its port
  /// the input again if anything the mapper reads is different.
  void refreshInput(VolumeSink* sink);
  /// Something mapper-level changed on a member: clipping planes, sampling,
  /// or the lead's normals.
  void refreshSettings();

  /// True while the multi-volume is what draws the members.
  bool active() const { return m_active; }
  /// The member at port 0, whose property decides shading for the set.
  VolumeSink* lead() const;
  /// The members, earliest first.
  QList<VolumeSink*> members() const { return m_members; }
  vtkSMViewProxy* view() const;

  vtkMultiVolume* multiVolume() const;
  vtkGPUVolumeRayCastMapper* mapper() const;

signals:
  void activeChanged(bool active);

private:
  explicit MultiVolumeCoordinator(vtkSMViewProxy* view);
  MultiVolumeCoordinator(const MultiVolumeCoordinator&) = delete;
  void operator=(const MultiVolumeCoordinator&) = delete;

  struct Member
  {
    int port = -1;
    /// The shallow copy handed to the mapper, and what it was made from.
    vtkSmartPointer<vtkImageData> input;
    vtkImageData* source = nullptr;
    QString array;
    std::array<double, 3> origin{};
    std::array<double, 3> spacing{};
    std::array<int, 6> extent{};
  };

  void activate();
  void deactivate();
  /// Swap the view's light kit for a headlight (the only lighting the
  /// multi-volume shader shades under), or put the kit back.
  void useHeadlight(bool on);
  /// A mapper set up the way the multi-volume needs it.
  static vtkSmartPointer<MultiVolumeMapper> makeMapper();
  /// Replace the mapper if its GPU resources were released since it was
  /// made (see the class comment), carrying every input and setting over.
  void ensureFreshMapper();
  /// Connect the member's current input to its port and register its prop.
  void attach(VolumeSink* sink);
  void detach(int port);
  int lowestFreePort() const;
  /// Tell every member (and @a also, if given) where it now stands.
  void notifyMembers(VolumeSink* also = nullptr);

  vtkWeakPointer<vtkSMViewProxy> m_view;
  /// The registry key; the weak pointer above goes null when the view dies.
  vtkSMViewProxy* m_viewKey = nullptr;
  vtkWeakPointer<vtkPVRenderView> m_renderView;
  vtkNew<vtkMultiVolume> m_multiVolume;
  vtkSmartPointer<MultiVolumeMapper> m_mapper;
  /// Watches the renderer start a frame, to swap a released mapper out
  /// before it draws.
  vtkNew<vtkCallbackCommand> m_renderObserver;
  unsigned long m_renderObserverId = 0;
  /// Stands in for the light kit while the multi-volume is active.
  vtkNew<vtkLight> m_headlight;
  bool m_lightKitSuspended = false;
  QList<VolumeSink*> m_members;
  QHash<VolumeSink*, Member> m_entries;
  bool m_active = false;
};

} // namespace pipeline
} // namespace tomviz

#endif
