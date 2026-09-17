/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizActiveObjects_h
#define tomvizActiveObjects_h

#include <QObject>

#include <vtkVector.h>

class pqRenderView;
class pqTimeKeeper;
class pqView;
class vtkSMSessionProxyManager;
class vtkSMViewProxy;

namespace tomviz {

namespace pipeline {
class Link;
class Pipeline;
class Node;
class OutputPort;
} // namespace pipeline

/// ActiveObjects keeps track of active objects in tomviz.
/// This is similar to pqActiveObjects in ParaView, however it tracks objects
/// relevant to tomviz.
class ActiveObjects : public QObject
{
  Q_OBJECT

public:
  /// Returns reference to the singleton instance.
  static ActiveObjects& instance();

  /// Returns the active view.
  vtkSMViewProxy* activeView() const;

  /// Returns the active view as a pqView object.
  pqView* activePqView() const;

  /// Returns the active pqRenderView object.
  pqRenderView* activePqRenderView() const;

  /// Returns the vtkSMSessionProxyManager from the active server/session.
  /// Provided here for convenience, since we need to access the proxy manager
  /// often.
  vtkSMSessionProxyManager* proxyManager() const;

  /// Returns the active time keeper
  pqTimeKeeper* activeTimeKeeper() const;

  /// Pipeline (single, set once at startup)
  void setPipeline(pipeline::Pipeline* p);
  pipeline::Pipeline* pipeline() const;

  /// Active selection tracking. At most one of node / port / link is
  /// non-null at a time — setting any of them to a non-null value clears
  /// the others. Use clearActiveSelection() to clear them all at once.
  void setActiveNode(pipeline::Node* node);
  pipeline::Node* activeNode() const;
  void setActivePort(pipeline::OutputPort* port);
  pipeline::OutputPort* activePort() const;
  void setActiveLink(pipeline::Link* link);
  pipeline::Link* activeLink() const;
  void clearActiveSelection();
  pipeline::OutputPort* activeTipOutputPort() const;

public slots:
  /// Set the active view;
  void setActiveView(vtkSMViewProxy*);

  /// Create a render view if needed.
  void createRenderViewIfNeeded();

  /// Set first existing render view to be active.
  void setActiveViewToFirstRenderView();

  /// Renders all views.
  void renderAllViews();

  /// Set whether to enable time series animations.
  void enableTimeSeriesAnimations(bool b);
  void setShowTimeSeriesLabel(bool b);

  bool timeSeriesAnimationsEnabled() const
  {
    return m_timeSeriesAnimationsEnabled;
  }
  bool showTimeSeriesLabel() const { return m_showTimeSeriesLabel; }

signals:
  /// Fired whenever the active view changes.
  void viewChanged(vtkSMViewProxy*);

  /// Fired when time series animations enable state is changed.
  void timeSeriesAnimationsEnableStateChanged(bool b);

  /// Fired when time series label visibility changes
  void showTimeSeriesLabelChanged(bool b);

  /// Fired as the mouse moves over a slice, with the voxel index and
  /// value under the cursor.
  void mouseOverVoxel(const vtkVector3i& ijk, double value);

  /// Fired whenever the active pipeline changes.
  void activePipelineChanged(pipeline::Pipeline*);

  /// Fired whenever the active node changes.
  void activeNodeChanged(pipeline::Node*);

  /// Fired whenever the active output port changes.
  void activePortChanged(pipeline::OutputPort*);

  /// Fired whenever the active link changes.
  void activeLinkChanged(pipeline::Link*);

  /// Fired whenever the active tip output port changes.
  void activeTipOutputPortChanged(pipeline::OutputPort*);

private slots:
  void viewChanged(pqView*);

protected:
  ActiveObjects();
  ~ActiveObjects() override;

  pipeline::Pipeline* m_pipeline = nullptr;
  pipeline::Node* m_activeNode = nullptr;
  pipeline::OutputPort* m_activePort = nullptr;
  pipeline::Link* m_activeLink = nullptr;
  pipeline::OutputPort* m_activeTipOutputPort = nullptr;

  /// Time series
  bool m_timeSeriesAnimationsEnabled = true;
  bool m_showTimeSeriesLabel = true;

private:
  void setActiveTipOutputPort(pipeline::OutputPort* port);
  // Tracks the active tip port's effectiveTypeChanged subscription so we
  // re-emit activeTipOutputPortChanged when type inference updates the tip
  // (e.g. a freshly-added transform whose output type is inferred from its
  // input only after the upstream link is created).
  QMetaObject::Connection m_tipEffectiveTypeConn;
  Q_DISABLE_COPY(ActiveObjects)
};

inline void ActiveObjects::enableTimeSeriesAnimations(bool b)
{
  if (m_timeSeriesAnimationsEnabled == b) {
    return;
  }

  m_timeSeriesAnimationsEnabled = b;
  emit timeSeriesAnimationsEnableStateChanged(b);
}

inline void ActiveObjects::setShowTimeSeriesLabel(bool b)
{
  if (m_showTimeSeriesLabel == b) {
    return;
  }

  m_showTimeSeriesLabel = b;
  emit showTimeSeriesLabelChanged(b);
}

} // namespace tomviz

#endif
