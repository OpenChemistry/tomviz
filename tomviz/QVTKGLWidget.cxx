/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "QVTKGLWidget.h"

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>

#include <QSurfaceFormat>
namespace tomviz {

QVTKGLWidget::QVTKGLWidget(QWidget* parent, Qt::WindowFlags f)
  : QVTKOpenGLWidget(parent, f)
{
  // Set some defaults for our render window.
  vtkNew<vtkGenericOpenGLRenderWindow> window;
  SetRenderWindow(window);
  QSurfaceFormat glFormat = QVTKOpenGLWidget::defaultFormat();
  glFormat.setSamples(8);
  setFormat(glFormat);
}

QVTKGLWidget::~QVTKGLWidget() = default;

} // namespace tomviz
