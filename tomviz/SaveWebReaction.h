/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSaveWebReaction_h
#define tomvizSaveWebReaction_h

#include <pqReaction.h>

namespace tomviz {

class MainWindow;

/// SaveWebReaction handles the "Save Web" action in tomviz. On trigger,
/// this will save several files for web viewing.
class SaveWebReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveWebReaction(QAction* parentAction, MainWindow* mainWindow);

  /// Save the file
  void saveWeb(QMap<QString, QVariant> kwargs);

protected:
  /// Called when the data changes to enable/disable the menu item
  void updateEnableState() override;

  /// Called when the action is triggered.
  void onTriggered() override;

private:
  MainWindow* m_mainWindow;
};
} // namespace tomviz
#endif
