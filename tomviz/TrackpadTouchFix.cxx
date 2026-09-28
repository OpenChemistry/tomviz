/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TrackpadTouchFix.h"

#include <QEvent>
#include <QWidget>

namespace tomviz {

#ifdef Q_OS_MAC

namespace {

// Qt recreates a gesture's state lazily, and the recognizers set the touch
// attributes again each time they do, so clearing the attributes once is not
// enough on its own. Drop the events on the way in as well.
class TouchEventFilter : public QObject
{
public:
  TouchEventFilter(QObject* parent) : QObject(parent) {}

protected:
  bool eventFilter(QObject*, QEvent* event) override
  {
    switch (event->type()) {
      case QEvent::TouchBegin:
      case QEvent::TouchUpdate:
      case QEvent::TouchEnd:
      case QEvent::TouchCancel:
        event->accept();
        return true;
      default:
        return false;
    }
  }
};

} // namespace

void disableTrackpadTouchEvents(QWidget* widget)
{
  if (!widget) {
    return;
  }

  // Pinch and swipe reach us as QNativeGestureEvent on macOS rather than as
  // touch events, so leaving those grabbed keeps trackpad zooming working.
  widget->ungrabGesture(Qt::PanGesture);
  widget->ungrabGesture(Qt::TapGesture);
  widget->ungrabGesture(Qt::TapAndHoldGesture);

  widget->setAttribute(Qt::WA_AcceptTouchEvents, false);
  widget->setAttribute(Qt::WA_TouchPadAcceptSingleTouchEvents, false);

  widget->installEventFilter(new TouchEventFilter(widget));
}

#else

void disableTrackpadTouchEvents(QWidget*) {}

#endif

} // namespace tomviz
