// License?

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "pqFiltersMenuReaction.h"
#include "pqLoadDataReaction.h"
#include "pqParaViewBehaviors.h"
#include "pqProxyGroupMenuManager.h"
#include "pqProxyGroupMenuManager.h"
#include "pqPVApplicationCore.h"
#include "pqPythonShellReaction.h"
#include "pqSaveDataReaction.h"

namespace TEM
{

class MainWindow::MWInternals
{
public:
  Ui::MainWindow Ui;
};


MainWindow::MainWindow(QWidget* _parent, Qt::WindowFlags _flags)
  : Superclass(_parent, _flags),
  Internals(new MainWindow::MWInternals())
{
  Ui::MainWindow& ui = this->Internals->Ui;
  ui.setupUi(this);

  new pqParaViewBehaviors(this, this);

  pqProxyGroupMenuManager* mgr =
    new pqProxyGroupMenuManager(ui.menuFilters, QString("NotUsed"));
  mgr->setRecentlyUsedMenuSize(0);
  pqPVApplicationCore::instance()->registerForQuicklaunch(
    mgr->widgetActionsHolder());
  new pqFiltersMenuReaction(mgr);

  mgr->addProxy("filters", "Clip");
  mgr->addProxy("filters", "Contour");
  mgr->addProxy("filters", "Cut");
  mgr->populateMenu();

  new pqLoadDataReaction(ui.actionOpen);
  new pqSaveDataReaction(ui.actionSave);
  QObject::connect(ui.actionExit, SIGNAL(triggered()),
    pqApplicationCore::instance(), SLOT(quit()));

  new pqPythonShellReaction(ui.actionPythonConsole);
}

MainWindow::~MainWindow()
{
  delete this->Internals;
}


}
