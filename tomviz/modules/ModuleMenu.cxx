/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleMenu.h"

#include "ActiveObjects.h"
#include "ModuleFactory.h"
#include "ModuleManager.h"

#include <QMenu>
#include <QToolBar>

namespace tomviz {

ModuleMenu::ModuleMenu(QToolBar* toolBar, QMenu* menu, QObject* parentObject)
  : QObject(parentObject), m_menu(menu), m_toolBar(toolBar)
{
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);
  connect(menu, SIGNAL(triggered(QAction*)), SLOT(triggered(QAction*)));
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateActions()));
  connect(&ActiveObjects::instance(),
          SIGNAL(moleculeSourceChanged(MoleculeSource*)),
          SLOT(updateActions()));
  updateActions();
}

ModuleMenu::~ModuleMenu() {}

void ModuleMenu::updateActions()
{
  QMenu* menu = m_menu;
  QToolBar* toolBar = m_toolBar;
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);

  menu->clear();
  toolBar->clear();

  auto activeDataSource = ActiveObjects::instance().activeDataSource();
  auto activeMoleculeSource = ActiveObjects::instance().activeMoleculeSource();
  auto activeView = ActiveObjects::instance().activeView();
  QList<QString> modules = ModuleFactory::moduleTypes();

  if (modules.size() > 0) {
    foreach (const QString& txt, modules) {
      auto actn = menu->addAction(ModuleFactory::moduleIcon(txt), txt);
      actn->setEnabled(
        ModuleFactory::moduleApplicable(txt, activeDataSource, activeView) ||
        ModuleFactory::moduleApplicable(txt, activeMoleculeSource, activeView));
      toolBar->addAction(actn);
      actn->setData(txt);
    }
  } else {
    auto action = menu->addAction("No modules available");
    action->setEnabled(false);
    toolBar->addAction(action);
  }
}

void ModuleMenu::triggered(QAction* maction)
{
  auto type = maction->data().toString();
  auto dataSource = ActiveObjects::instance().activeDataSource();
  auto moleculeSource = ActiveObjects::instance().activeMoleculeSource();
  auto view = ActiveObjects::instance().activeView();

  Module* module;
  if (type == "Molecule") {
    module =
      ModuleManager::instance().createAndAddModule(type, moleculeSource, view);
  } else {
    module =
      ModuleManager::instance().createAndAddModule(type, dataSource, view);
  }

  if (module) {
    ActiveObjects::instance().setActiveModule(module);
  } else {
    qCritical("Failed to create requested module.");
  }
}

} // end of namespace tomviz
