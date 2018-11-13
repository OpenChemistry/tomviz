/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSetDataTypeReaction_h
#define tomvizSetDataTypeReaction_h

#include <pqReaction.h>

#include "DataSource.h"

class QMainWindow;

namespace tomviz {

class DataSource;

class SetDataTypeReaction : public pqReaction
{
  Q_OBJECT

public:
  SetDataTypeReaction(QAction* action, QMainWindow* mw,
                      DataSource::DataSourceType t = DataSource::Volume);

  static void setDataType(QMainWindow* mw, DataSource* source = nullptr,
                          DataSource::DataSourceType t = DataSource::Volume);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;
  void updateEnableState() override;

private:
  QMainWindow* m_mainWindow;
  DataSource::DataSourceType m_type;

  void setWidgetText(DataSource::DataSourceType t);

  Q_DISABLE_COPY(SetDataTypeReaction)
};
} // namespace tomviz

#endif
