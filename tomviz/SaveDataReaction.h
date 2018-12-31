/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSaveDataReaction_h
#define tomvizSaveDataReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;
class PythonWriterFactory;

/// SaveDataReaction handles the "Save Data" action in tomviz. On trigger,
/// this will save the data file.
class SaveDataReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveDataReaction(QAction* parentAction);

  /// Save the file
  bool saveData(const QString& filename);

protected:
  /// Called when the data changes to enable/disable the menu item
  void updateEnableState() override;

  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(SaveDataReaction)
};
} // namespace tomviz
#endif
