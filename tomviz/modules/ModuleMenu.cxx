/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleMenu.h"

#include "ActiveObjects.h"
#include "ModuleFactory.h"
#include "ModuleManager.h"

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqServer.h>
#include <pqServerManagerModel.h>
#include <pqView.h>
#include <vtkSMContextViewProxy.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMViewLayoutProxy.h>
#include <vtkSMViewProxy.h>

#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QToolBar>

namespace tomviz {

static vtkSMViewProxy* resolveView(const QString& moduleType)
{
  // Determine which view type this module needs.
  bool needsChart = (moduleType == "Plot");
  QString viewTypeName = needsChart ? "XYChartView" : "RenderView";

  // Check if the active view is already the right type.
  auto* activeView = ActiveObjects::instance().activeView();
  if (activeView) {
    bool activeIsChart = vtkSMContextViewProxy::SafeDownCast(activeView);
    bool activeIsRender = vtkSMRenderViewProxy::SafeDownCast(activeView);
    if ((needsChart && activeIsChart) || (!needsChart && activeIsRender)) {
      return activeView;
    }
  }

  // Enumerate all views and find matching ones.
  auto* smModel =
    pqApplicationCore::instance()->getServerManagerModel();
  QList<pqView*> allViews = smModel->findItems<pqView*>();

  QList<pqView*> matching;
  for (auto* v : allViews) {
    auto* proxy = v->getViewProxy();
    bool isChart = vtkSMContextViewProxy::SafeDownCast(proxy);
    bool isRender = vtkSMRenderViewProxy::SafeDownCast(proxy);
    if ((needsChart && isChart) || (!needsChart && isRender)) {
      matching.append(v);
    }
  }

  if (matching.isEmpty()) {
    // No matching view exists — ask to create one.
    QString label = needsChart ? "Line Chart View" : "Render View";
    auto answer = QMessageBox::question(
      nullptr, "Create View?",
      QString("No %1 is available. Create one?").arg(label),
      QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      return nullptr;
    }
    // Split the current layout to make room for the new view.
    int emptyCell = -1;
    vtkSMViewLayoutProxy* layout = nullptr;
    if (activeView) {
      layout = vtkSMViewLayoutProxy::FindLayout(activeView);
      if (layout) {
        int location = layout->GetViewLocation(activeView);
        // Split returns index of left child; existing view moves there.
        // The empty cell is at leftChild + 1.
        int leftChild = layout->Split(
          location, vtkSMViewLayoutProxy::HORIZONTAL, 0.5);
        if (leftChild >= 0) {
          emptyCell = leftChild + 1;
        }
      }
    }

    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    auto* server = pqApplicationCore::instance()->getActiveServer();
    auto* newView = builder->createView(viewTypeName, server);
    if (!newView) {
      return nullptr;
    }
    auto* proxy = newView->getViewProxy();

    // Explicitly assign the new view to the empty cell we created.
    if (layout && emptyCell >= 0) {
      layout->AssignView(emptyCell, proxy);
    }

    ActiveObjects::instance().setActiveView(proxy);
    return proxy;
  }

  if (matching.size() == 1) {
    auto* proxy = matching.first()->getViewProxy();
    ActiveObjects::instance().setActiveView(proxy);
    return proxy;
  }

  // Multiple matches — let the user pick.
  QStringList names;
  for (auto* v : matching) {
    names << v->getSMName();
  }
  bool ok = false;
  QString chosen = QInputDialog::getItem(
    nullptr, "Select View",
    QString("Multiple views available. Select one:"),
    names, 0, false, &ok);
  if (!ok) {
    return nullptr;
  }
  int idx = names.indexOf(chosen);
  auto* proxy = matching[idx]->getViewProxy();
  ActiveObjects::instance().setActiveView(proxy);
  return proxy;
}

ModuleMenu::ModuleMenu(QToolBar* toolBar, QMenu* menu, QObject* parentObject)
  : QObject(parentObject), m_menu(menu), m_toolBar(toolBar)
{
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);
  connect(menu, &QMenu::triggered, this, &ModuleMenu::triggered);
  connect(&ActiveObjects::instance(), QOverload<DataSource*>::of(&ActiveObjects::dataSourceChanged), this, &ModuleMenu::updateActions);
  connect(&ActiveObjects::instance(), &ActiveObjects::moleculeSourceChanged, this, &ModuleMenu::updateActions);
  connect(&ActiveObjects::instance(), &ActiveObjects::resultChanged, this, &ModuleMenu::updateActions);
  connect(&ActiveObjects::instance(), QOverload<vtkSMViewProxy*>::of(&ActiveObjects::viewChanged), this, &ModuleMenu::updateActions);
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
  auto activeOperatorResult = ActiveObjects::instance().activeOperatorResult();
  auto activeView = ActiveObjects::instance().activeView();
  QList<QString> modules = ModuleFactory::moduleTypes();

  if (modules.size() > 0) {
    foreach (const QString& txt, modules) {
      auto actn = menu->addAction(ModuleFactory::moduleIcon(txt), txt);
      actn->setEnabled(
        ModuleFactory::moduleApplicable(txt, activeDataSource, activeView) ||
        ModuleFactory::moduleApplicable(txt, activeMoleculeSource, activeView) ||
        ModuleFactory::moduleApplicable(txt, activeOperatorResult, activeView));
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
  auto operatorResult = ActiveObjects::instance().activeOperatorResult();

  auto* view = resolveView(type);
  if (!view) {
    return;
  }

  Module* module;
  if (type == "Molecule") {
    if (operatorResult) {
      module =
        ModuleManager::instance().createAndAddModule(type, operatorResult, view);
    } else {
      module =
        ModuleManager::instance().createAndAddModule(type, moleculeSource, view);
    }
  } else if (type == "Plot") {
    module =
        ModuleManager::instance().createAndAddModule(type, operatorResult, view);
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
