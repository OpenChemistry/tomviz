/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizResetReaction_h
#define tomvizResetReaction_h

#include <pqReaction.h>

namespace tomviz {
class ResetReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  ResetReaction(QAction* action);

  static void reset();

  void updateEnableState() override;

protected:
  void onTriggered() override { reset(); }

private:
  Q_DISABLE_COPY(ResetReaction)
};
} // namespace tomviz
#endif
