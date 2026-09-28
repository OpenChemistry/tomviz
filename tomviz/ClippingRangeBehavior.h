/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizClippingRangeBehavior_h
#define tomvizClippingRangeBehavior_h

#include <QObject>

class pqView;
class vtkSMViewProxy;

namespace tomviz {

/// Keeps every render view's camera clipping range fitted to what it
/// actually shows.
///
/// ParaView clamps the clipping range to the view's cached scene bounds
/// on every render (vtkPVRenderView::Render) and whenever anything resets
/// it. That cache is only refreshed by a view update, which ParaView
/// triggers for its own representations, but tomviz visualizations add
/// their props to the renderer directly, so data that grows or moves
/// (a live source, a transform, a new dataset) kept being cut by stale
/// near and far planes. This refreshes the cache at the start of every
/// render of every render view.
class ClippingRangeBehavior : public QObject
{
  Q_OBJECT

public:
  explicit ClippingRangeBehavior(QObject* parent = nullptr);

  /// Refresh @a view's bounds before each of its renders. Not a render
  /// view: nothing happens.
  static void watch(vtkSMViewProxy* view);

private:
  void watchView(pqView* view);
};

} // namespace tomviz

#endif
