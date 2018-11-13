/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRecentFilesMenu_h
#define tomvizRecentFilesMenu_h

#include <QObject>

class QAction;
class QMenu;

namespace tomviz {

class DataSource;
class MoleculeSource;

/// Adds recent file and recent state file support to Tomviz.
class RecentFilesMenu : public QObject
{
  Q_OBJECT

public:
  RecentFilesMenu(QMenu& menu, QObject* parent = nullptr);
  ~RecentFilesMenu() override;

  /// Pushes a reader on the recent files stack.
  static void pushDataReader(DataSource* dataSource);
  static void pushMoleculeReader(MoleculeSource* moleculeSource);
  static void pushStateFile(const QString& filename);

private slots:
  void aboutToShowMenu();
  void dataSourceTriggered(QAction* actn, QStringList fileNames);
  void moleculeSourceTriggered(QAction* actn, QString fileName);
  void stateTriggered();

private:
  Q_DISABLE_COPY(RecentFilesMenu)
};
} // namespace tomviz
#endif
