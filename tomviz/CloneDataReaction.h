/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCloneDataReaction_h
#define tomvizCloneDataReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;

class CloneDataReaction : public pqReaction
{
  Q_OBJECT

public:
  CloneDataReaction(QAction* action);

  static DataSource* clone(DataSource* toClone = nullptr);

protected:
  /// Called when the action is triggered.
  void onTriggered() override { clone(); }
  void updateEnableState() override;

private:
  Q_DISABLE_COPY(CloneDataReaction)
};
} // namespace tomviz

#endif
