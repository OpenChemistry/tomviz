/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAnimationHelperDialog_h
#define tomvizAnimationHelperDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace tomviz {

class AnimationHelperDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AnimationHelperDialog(QWidget* parent);
  ~AnimationHelperDialog() override;

private:
  Q_DISABLE_COPY(AnimationHelperDialog)

  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz

#endif
