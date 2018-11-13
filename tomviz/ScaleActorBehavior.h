/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizScaleActorBehavior_h
#define tomvizScaleActorBehavior_h

#include <QObject>

class pqView;

namespace tomviz {
class ScaleActorBehavior : public QObject
{
  Q_OBJECT

public:
  ScaleActorBehavior(QObject* parent = nullptr);

private slots:
  void viewAdded(pqView*);

private:
  Q_DISABLE_COPY(ScaleActorBehavior)
};
} // namespace tomviz

#endif
