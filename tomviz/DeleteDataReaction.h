/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDeleteDataReaction_h
#define tomvizDeleteDataReaction_h

#include <pqReaction.h>

#include <QPointer>

class vtkSMProxy;

namespace tomviz {
class DataSource;

/// DeleteDataReaction handles the "Delete Data" action in tomviz. On trigger,
/// this will delete the active data source and all Modules connected to it.
class DeleteDataReaction : public pqReaction
{
  Q_OBJECT

public:
  DeleteDataReaction(QAction* parentAction);

  /// Create a raw data source from the reader.
  static void deleteDataSource(DataSource*);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;
  void updateEnableState() override;

private slots:
  void activeDataSourceChanged();

private:
  Q_DISABLE_COPY(DeleteDataReaction)

  QPointer<DataSource> m_activeDataSource;
};
} // namespace tomviz
#endif
