/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizQVTKGLWidget_h
#define tomvizQVTKGLWidget_h

#include <QVTKOpenGLWidget.h>

namespace tomviz {

class QVTKGLWidget : public QVTKOpenGLWidget
{
  Q_OBJECT

public:
  QVTKGLWidget(QWidget* parent = nullptr,
               Qt::WindowFlags f = Qt::WindowFlags());
  ~QVTKGLWidget() override;

private:
};
} // namespace tomviz

#endif // tomvizQVTKGLWidget_h
