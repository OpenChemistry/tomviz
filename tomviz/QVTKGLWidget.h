/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizQVTKGLWidget_h
#define tomvizQVTKGLWidget_h

#include <QVTKOpenGLNativeWidget.h>

namespace tomviz {

class QVTKGLWidget : public QVTKOpenGLNativeWidget
{
  Q_OBJECT

public:
  QVTKGLWidget(QWidget* parent = nullptr,
               Qt::WindowFlags f = Qt::WindowFlags());
  ~QVTKGLWidget() override;

protected:
  bool event(QEvent* evt) override;
};
} // namespace tomviz

#endif // tomvizQVTKGLWidget_h
