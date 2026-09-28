/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizImageViewerModeBehavior_h
#define tomvizImageViewerModeBehavior_h

#include <QObject>

#include <map>
#include <memory>

class pqRenderView;
class pqView;

namespace tomviz {

namespace pipeline {
class SliceSink;
}

/// Turns a render view into a 2D image viewer while its interaction mode
/// is 2D, and puts it back when the mode returns to 3D.
///
/// Hangs off the 2D/3D toggle in every render view's toolbar, or any
/// other change of the view's InteractionMode property. On entering 2D
/// every non-slice visualization in that view is hidden, one slice in
/// the view is chosen (the one fed by the active tip output port when
/// there are several, a new one on the tip port when there is none) and
/// the camera is aimed straight down its normal. On returning to 3D the
/// hidden visualizations, the slice this behavior showed or created, and
/// the camera (position, direction and up) are restored.
///
/// Everything is per view: a split layout can hold a 2D view next to a
/// 3D one, and views showing other datasets are left alone.
class ImageViewerModeBehavior : public QObject
{
  Q_OBJECT

public:
  explicit ImageViewerModeBehavior(QObject* parent = nullptr);
  ~ImageViewerModeBehavior() override;

  /// Whether @a view is currently set up as an image viewer.
  bool isActive(pqRenderView* view) const;

  /// The slice the image viewer in @a view looks at, or nullptr when the
  /// view is not an image viewer (or the slice was deleted since).
  pipeline::SliceSink* slice(pqRenderView* view) const;

private:
  struct State;

  void watch(pqView* view);
  void onInteractionModeChanged(pqRenderView* view, int mode);
  void enter(pqRenderView* view);
  void leave(pqRenderView* view);

  std::map<pqRenderView*, std::unique_ptr<State>> m_states;

  Q_DISABLE_COPY(ImageViewerModeBehavior)
};

} // namespace tomviz

#endif
