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
#include "CloneDataReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include <pqCoreUtilities.h>

#include <QInputDialog>

namespace tomviz
{

CloneDataReaction::CloneDataReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateEnableState()));
  this->updateEnableState();
}

CloneDataReaction::~CloneDataReaction()
{
}

void CloneDataReaction::updateEnableState()
{
  this->parentAction()->setEnabled(
    ActiveObjects::instance().activeDataSource() != nullptr);
}

DataSource* CloneDataReaction::clone(DataSource* toClone)
{
  toClone = toClone? toClone : ActiveObjects::instance().activeDataSource();
  if (!toClone)
  {
    return nullptr;
  }

  QStringList items;
  items << "Original data only"
        << "Original data with transformations"
        << "Transformed data only";

  bool user_okayed;
  QString	selection = QInputDialog::getItem(
    pqCoreUtilities::mainWidget(),
    "Clone Data Options",
    "Select what should be cloned",
    items,
    /*current=*/0,
    /*editable=*/false,
    /*ok*/&user_okayed);

  if (user_okayed)
  {
    DataSource* newClone = toClone->clone(selection == items[1],
                                          selection == items[2]);
    LoadDataReaction::dataSourceAdded(newClone);
    return newClone;
  }
  return nullptr;
}

}
