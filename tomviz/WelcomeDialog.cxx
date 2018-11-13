/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "WelcomeDialog.h"
#include "ui_WelcomeDialog.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "ModuleManager.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

namespace tomviz {

WelcomeDialog::WelcomeDialog(MainWindow* mw)
  : QDialog(mw), m_ui(new Ui::WelcomeDialog)
{
  m_ui->setupUi(this);
  connect(m_ui->doNotShowAgain, SIGNAL(stateChanged(int)),
          SLOT(onDoNotShowAgainStateChanged(int)));
  connect(m_ui->noButton, SIGNAL(clicked()), SLOT(hide()));
  connect(m_ui->yesButton, SIGNAL(clicked()), SLOT(onLoadSampleDataClicked()));
}

WelcomeDialog::~WelcomeDialog() = default;

void WelcomeDialog::onLoadSampleDataClicked()
{
  auto mw = qobject_cast<MainWindow*>(this->parent());
  mw->openRecon();
  auto& mm = ModuleManager::instance();
  auto& ao = ActiveObjects::instance();
  // Remove the orthogonal slice that is automatically created
  mm.removeModule(ao.activeModule());
  // Add a volume module
  if (auto module = mm.createAndAddModule("Volume", ao.activeDataSource(),
                                          ao.activeView())) {
    ao.setActiveModule(module);
  }
  hide();
}

void WelcomeDialog::onDoNotShowAgainStateChanged(int state)
{
  bool showDialog = (state != Qt::Checked);

  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("GeneralSettings.ShowWelcomeDialog", showDialog);
}
} // namespace tomviz
