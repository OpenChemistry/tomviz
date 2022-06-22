/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataBrokerSaveReaction_h
#define tomvizDataBrokerSaveReaction_h

#include <pqReaction.h>

#include "DataBroker.h"
#include "MainWindow.h"

namespace tomviz {
class DataSource;

/// DataBrokerSaveReaction handles the "Export to DataBroker" action in
/// tomviz.
///
class DataBrokerSaveReaction : public pqReaction
{
  Q_OBJECT

public:
  DataBrokerSaveReaction(QAction* parentAction, MainWindow* mainWindow);
  ~DataBrokerSaveReaction() override;

  void setDataBrokerInstalled(bool installed);
  void saveData();

protected:
  /// Called when the action is triggered.
  void onTriggered() override;

  bool m_dataBrokerInstalled = false;

private:
  Q_DISABLE_COPY(DataBrokerSaveReaction)
  MainWindow* m_mainWindow;
};
} // namespace tomviz

#endif
