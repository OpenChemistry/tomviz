/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLoadTimeSeriesReaction_h
#define tomvizLoadTimeSeriesReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;

/// LoadTimeSeriesReaction handles the "Open Time Series" action in tomviz. On
/// trigger, this will open the data files and perform the necessary subsequent
/// actions.
class LoadTimeSeriesReaction : public pqReaction
{
  Q_OBJECT

public:
  LoadTimeSeriesReaction(QAction* parentAction);
  ~LoadTimeSeriesReaction() override;

  static QList<DataSource*> loadData();

protected:
  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(LoadTimeSeriesReaction)
};
} // namespace tomviz

#endif
