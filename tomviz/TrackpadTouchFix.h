/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTrackpadTouchFix_h
#define tomvizTrackpadTouchFix_h

class QWidget;

namespace tomviz {

/// Stop a macOS trackpad from looking like a held left mouse button in a VTK
/// widget.
///
/// QVTKOpenGLNativeWidget's constructor calls grabGesture(Qt::PanGesture).
/// On macOS Qt's pan recognizer is touch based, so grabbing it sets both
/// Qt::WA_AcceptTouchEvents and Qt::WA_TouchPadAcceptSingleTouchEvents on the
/// widget. The latter is what makes Qt stop discarding single-point touchpad
/// events, so a single finger merely moving across the trackpad starts
/// delivering QTouchEvents even though nothing has been clicked.
///
/// QVTKInteractorAdapter::ProcessEvent then translates a touch point in the
/// pressed state into vtkCommand::LeftButtonPressEvent (and a released point
/// into LeftButtonReleaseEvent). Because a hovering finger produces presses
/// that never get a matching release, VTK is left believing the left button is
/// held down. In the 2D views that draws rubber band rectangles
/// (vtkInteractorStyleRubberBand2D) and drags histogram contour markers.
///
/// This is a no-op off macOS, where the touch to left button mapping is the
/// intended behavior for real touchscreens.
void disableTrackpadTouchEvents(QWidget* widget);

} // namespace tomviz

#endif // tomvizTrackpadTouchFix_h
