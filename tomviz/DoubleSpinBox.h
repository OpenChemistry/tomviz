/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDoubleSpinBox_h
#define tomvizDoubleSpinBox_h

#include <QDoubleSpinBox>

/*
 * This class is a QDoubleSpinBox that fires its editingFinished() signal
 * whenever the value is modified from the up and down arrow buttons in
 * addition to when it loses focus.  We want to update in response to both of
 * these.
 */

namespace tomviz {

class DoubleSpinBox : public QDoubleSpinBox
{
  Q_OBJECT
public:
  DoubleSpinBox(QWidget* parent = nullptr);

protected:
  void mousePressEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);

private:
  bool pressInUp;
  bool pressInDown;
};
} // namespace tomviz

#endif
