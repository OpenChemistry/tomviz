/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleMenu_h
#define tomvizModuleMenu_h

#include <QObject>
#include <QPointer>

class QAction;
class QMenu;
class QToolBar;

namespace tomviz {

/// ModuleMenu is manager for the Modules menu. It fills it up with actions
/// and handles their triggers based on available modules reported by
/// ModuleFactory.
class ModuleMenu : public QObject
{
  Q_OBJECT

public:
  ModuleMenu(QToolBar* toolBar, QMenu* parentMenu, QObject* parent = nullptr);
  virtual ~ModuleMenu();

private slots:
  void updateActions();
  void triggered(QAction* maction);

private:
  Q_DISABLE_COPY(ModuleMenu)
  QPointer<QMenu> m_menu;
  QPointer<QToolBar> m_toolBar;
};
} // namespace tomviz

#endif
