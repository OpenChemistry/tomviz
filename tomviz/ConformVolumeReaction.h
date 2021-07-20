/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConformVolumeReaction_h
#define tomvizConformVolumeReaction_h

#include <pqReaction.h>

#include <QSet>

namespace tomviz {

class DataSource;

class ConformVolumeReaction : pqReaction
{
  Q_OBJECT

public:
  ConformVolumeReaction(QAction* action);

public slots:
  void updateDataSources(QSet<DataSource*>);

protected:
  void onTriggered() override;
  void updateEnableState() override;
  void updateVisibleState();

  DataSource* createConformedVolume();

private:
  Q_DISABLE_COPY(ConformVolumeReaction)

  QSet<DataSource*> m_dataSources;
  DataSource* m_conformingVolume = nullptr;
};
} // namespace tomviz

#endif
