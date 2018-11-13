/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMergeImagesReaction_h
#define tomvizMergeImagesReaction_h

#include <pqReaction.h>

#include <QSet>

namespace tomviz {

class DataSource;

class MergeImagesReaction : pqReaction
{
  Q_OBJECT

public:
  MergeImagesReaction(QAction* action);

public slots:
  void updateDataSources(QSet<DataSource*>);

protected:
  void onTriggered() override;
  void updateEnableState() override;

  DataSource* mergeArrays();
  DataSource* mergeComponents();

private:
  Q_DISABLE_COPY(MergeImagesReaction)

  QSet<DataSource*> m_dataSources;
};
} // namespace tomviz

#endif
