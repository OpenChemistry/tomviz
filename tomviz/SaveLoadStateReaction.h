/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSaveLoadStateReaction_h
#define tomvizSaveLoadStateReaction_h

#include <pqReaction.h>

#include <QByteArray>

namespace tomviz {

class SaveLoadStateReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveLoadStateReaction(QAction* action, bool load = false);

  static bool saveState();
  static bool saveState(const QString& filename, bool interactive = true);
  static bool loadState();
  static bool loadState(const QString& filename);

protected:
  void onTriggered() override;
  static bool automaticallyExecutePipelines();

private:
  Q_DISABLE_COPY(SaveLoadStateReaction)
  bool m_load;

  static bool checkForLegacyStateFileFormat(const QByteArray state);
  static QString extractLegacyStateFileVersion(const QByteArray state);

  static bool saveTvsm(const QString& filename, bool interactive = true);
  static bool saveTvh5(const QString& filename);

  static bool loadTvsm(const QString& filename);
  static bool loadTvh5(const QString& filename);
};
} // namespace tomviz

#endif
