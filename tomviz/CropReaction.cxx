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
#include "CropReaction.h"

#include <QAction>
#include <QMainWindow>

#include "ActiveObjects.h"
#include "CropOperator.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"

namespace tomviz {

CropReaction::CropReaction(QAction* parentObject, QMainWindow* mw)
  : pqReaction(parentObject), m_mainWindow(mw)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void CropReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

void CropReaction::crop(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeParentDataSource();
  if (!source) {
    return;
  }

  Operator* Op = new CropOperator();

  EditOperatorDialog* dialog =
    new EditOperatorDialog(Op, source, true, m_mainWindow);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  connect(Op, SIGNAL(destroyed()), dialog, SLOT(reject()));
}
} // namespace tomviz
