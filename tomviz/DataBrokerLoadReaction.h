/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataBrokerLoadReaction_h
#define tomvizDataBrokerLoadReaction_h

#include <pqReaction.h>

#include "DataBroker.h"

namespace tomviz {
class DataSource;

/// DataBrokerLoadReaction handles the "Import From DataBroker" action in
/// tomviz. On trigger, this will open a dialog where the user "drill down" to
/// the require variable, which is then loaded into Tomviz.
///
class DataBrokerLoadReaction : public pqReaction
{
  Q_OBJECT

public:
  DataBrokerLoadReaction(QAction* parentAction);
  ~DataBrokerLoadReaction() override;

  static void loadData();

protected:
  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(DataBrokerLoadReaction)
};
} // namespace tomviz

#endif
