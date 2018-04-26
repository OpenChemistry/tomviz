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
#include "Utilities.h"

#include <QInputDialog>

namespace tomviz {

CloneDataReaction::CloneDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void CloneDataReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

DataSource* CloneDataReaction::clone(DataSource* toClone)
{
  toClone = toClone ? toClone : ActiveObjects::instance().activeDataSource();
  if (!toClone) {
    return nullptr;
  }

  QStringList items;
  items << "Data only"
        << "Data with transformations";

  bool userOkayed;
  QString selection =
    QInputDialog::getItem(tomviz::mainWidget(), "Clone Data Options",
                          "Select what should be cloned", items,
                          /*current=*/0,
                          /*editable=*/false,
                          /*ok*/ &userOkayed);

  if (userOkayed) {
    DataSource* newClone = toClone->clone();
    LoadDataReaction::dataSourceAdded(newClone);
    auto cloneOperators = selection == items[1];
    // We clone the operators after adding the data source as at this point the
    // appropriate signals are connected on the data source.
    if (cloneOperators) {
      // now, clone the operators.
      foreach (Operator* op, toClone->operators()) {
        Operator* opClone(op->clone());
        newClone->addOperator(opClone);
      }
    }
    return newClone;
  }
  return nullptr;
}
}
