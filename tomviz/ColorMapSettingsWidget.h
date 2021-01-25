/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizColorMapSettingsWidget_h
#define tomvizColorMapSettingsWidget_h

#include <QScopedPointer>
#include <QWidget>

class vtkColorTransferFunction;

namespace tomviz {

class ColorMapSettingsWidget : public QWidget
{
  // Widget to edit some color map settings, such as color space.
  // Connect to the vtkColorTransferFunction Modified event to
  // be notified when updates occur.

  Q_OBJECT
  typedef QWidget Superclass;

public:
  ColorMapSettingsWidget(vtkColorTransferFunction* lut = nullptr,
                         QWidget* parent = nullptr);
  virtual ~ColorMapSettingsWidget();

  void setLut(vtkColorTransferFunction* lut);

  void updateGui();

private:
  class Internals;
  QScopedPointer<Internals> m_internals;
};
} // namespace tomviz

#endif
