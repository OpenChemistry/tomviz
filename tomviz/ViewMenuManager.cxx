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

#include "ViewMenuManager.h"

#include <QAction>
#include <QDialog>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>

#include "ViewPropertiesPanel.h"

namespace tomviz
{

ViewMenuManager::ViewMenuManager(QMainWindow* mainWindow, QMenu* menu)
  : pqViewMenuManager(mainWindow, menu)
{
  this->viewPropertiesDialog = new QDialog(mainWindow);
  this->viewPropertiesDialog->setWindowTitle("View Properties");
  ViewPropertiesPanel* panel = new ViewPropertiesPanel(this->viewPropertiesDialog);
  QHBoxLayout *layout = new QHBoxLayout;
  layout->addWidget(panel);
  this->viewPropertiesDialog->setLayout(layout);
  this->connect(this->viewPropertiesDialog, SIGNAL(finished(int)),
                    SLOT(viewPropertiesDialogHidden()));

  this->showViewPropertiesAction = new QAction("View Properties", this->Menu);
  this->showViewPropertiesAction->setCheckable(true);
  this->connect(this->showViewPropertiesAction, SIGNAL(triggered(bool)),
                    SLOT(showViewPropertiesDialog(bool)));
}

void ViewMenuManager::buildMenu()
{
  bool checked = this->showViewPropertiesAction->isChecked();
  this->showViewPropertiesAction = NULL; // The object is about to be deleted
  pqViewMenuManager::buildMenu(); // deletes all prior menu items and repopulates menu

  this->showViewPropertiesAction = new QAction("View Properties", this->Menu);
  this->showViewPropertiesAction->setCheckable(true);
  this->showViewPropertiesAction->setChecked(checked);
  this->connect(this->showViewPropertiesAction, SIGNAL(triggered(bool)),
                    SLOT(showViewPropertiesDialog(bool)));
  this->Menu->addSeparator();
  this->Menu->addAction(this->showViewPropertiesAction);
}

void ViewMenuManager::showViewPropertiesDialog(bool show)
{
  if (show)
  {
    this->viewPropertiesDialog->show();
  }
  else
  {
    this->viewPropertiesDialog->accept();
  }
}

void ViewMenuManager::viewPropertiesDialogHidden()
{
  this->showViewPropertiesAction->setChecked(false);
}

}
