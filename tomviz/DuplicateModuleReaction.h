/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDuplicateModuleReaction_h
#define tomvizDuplicateModuleReaction_h

#include <pqReaction.h>

namespace tomviz {

/// SaveDataReaction handles the "Save Data" action in tomviz. On trigger,
/// this will save the data file.
class DuplicateModuleReaction : public pqReaction
{
  Q_OBJECT

public:
  DuplicateModuleReaction(QAction* parentAction);

protected:
  /// Called when the data changes to enable/disable the menu item
  void updateEnableState() override;

  /// Called when the action is triggered.
  void onTriggered() override;
};

} // namespace tomviz

#endif
