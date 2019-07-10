/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "QVTKGLWidget.h"

#include <QVTKRenderWindowAdapter.h>

#include <QEvent>
#include <QSurfaceFormat>

namespace tomviz {

QVTKGLWidget::QVTKGLWidget(QWidget* parent, Qt::WindowFlags f)
  : QVTKOpenGLNativeWidget(parent, f)
{
  // Set some defaults for our render window.
  QSurfaceFormat glFormat = QVTKOpenGLNativeWidget::defaultFormat();
  glFormat.setSamples(8);
  setFormat(glFormat);
}

QVTKGLWidget::~QVTKGLWidget() = default;

bool QVTKGLWidget::event(QEvent* e)
{
  if (RenderWindowAdapter) {
    RenderWindowAdapter->handleEvent(e);
  }
  // If they are mouse events then we should take them. This affects KDE on
  // Linux where not accepting them causes the window to be moved.
  const QEvent::Type t = e->type();
  if (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonRelease ||
      t == QEvent::MouseButtonDblClick || t == QEvent::MouseMove) {
    return true;
  } else {
    return QOpenGLWidget::event(e);
  }
}

} // namespace tomviz
