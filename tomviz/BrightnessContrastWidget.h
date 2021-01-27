/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizBrightnessContrastWidget_h
#define tomvizBrightnessContrastWidget_h

#include <QScopedPointer>
#include <QWidget>

class vtkDiscretizableColorTransferFunction;

namespace tomviz {

class DataSource;

class BrightnessContrastWidget : public QWidget
{
  // Widget to edit the brightness and contrast of a color map.
  // This is performed by moving the min and max around.
  // Connect to the vtkDiscretizableColorTransferFunction Modified event to
  // be notified when updates occur.

  Q_OBJECT
  typedef QWidget Superclass;

public:
  BrightnessContrastWidget(DataSource* ds,
                           vtkDiscretizableColorTransferFunction* lut,
                           QWidget* parent = nullptr);
  virtual ~BrightnessContrastWidget();

  void setDataSource(DataSource* ds);
  void setLut(vtkDiscretizableColorTransferFunction* lut);

  void updateGui();

signals:
  void autoPressed();
  void resetPressed();

private:
  class Internals;
  QScopedPointer<Internals> m_internals;
};
} // namespace tomviz

#endif
