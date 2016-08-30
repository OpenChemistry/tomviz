/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "WelcomeDialog.h"
#include "ui_WelcomeDialog.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "ModuleManager.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

namespace tomviz {

WelcomeDialog::WelcomeDialog(MainWindow* mw)
  : QDialog(mw), ui(new Ui::WelcomeDialog)
{
  ui->setupUi(this);
  this->connect(this->ui->doNotShowAgain, SIGNAL(stateChanged(int)), this,
                SLOT(onDoNotShowAgainStateChanged(int)));
  this->connect(this->ui->noButton, SIGNAL(clicked()), this, SLOT(hide()));
  this->connect(this->ui->yesButton, SIGNAL(clicked()), this,
                SLOT(onLoadSampleDataClicked()));
}

WelcomeDialog::~WelcomeDialog()
{
}

void WelcomeDialog::onLoadSampleDataClicked()
{
  MainWindow* mw = qobject_cast<MainWindow*>(this->parent());
  mw->openRecon();
  ModuleManager& mm = ModuleManager::instance();
  ActiveObjects& ao = ActiveObjects::instance();
  // Remove the orthogonal slice that is automatically created
  mm.removeModule(ao.activeModule());
  // Add a volume module
  if (Module* module = mm.createAndAddModule("Volume", ao.activeDataSource(),
                                             ao.activeView())) {
    ao.setActiveModule(module);
  }
  this->hide();
}

void WelcomeDialog::onDoNotShowAgainStateChanged(int state)
{
  bool showDialog = (state != Qt::Checked);

  QSettings* settings = pqApplicationCore::instance()->settings();
  settings->setValue("GeneralSettings.ShowWelcomeDialog", showDialog ? 1 : 0);
}
}
