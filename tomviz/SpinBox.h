/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSpinBox_h
#define tomvizSpinBox_h

#include <QSpinBox>

/*
 * This class is a QSpinBox that fires its editingFinished() signal whenever the
 * value is modified from the up and down arrow buttons in addition to when it
 * loses focus.  We want to update in response to both of these.
 */
namespace tomviz {

class SpinBox : public QSpinBox
{
  Q_OBJECT
public:
  SpinBox(QWidget* parent = nullptr);

protected:
  void mousePressEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);

private:
  bool pressInUp;
  bool pressInDown;
};
} // namespace tomviz

#endif
