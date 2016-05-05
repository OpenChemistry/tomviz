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
#include "AddAlignReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "TranslateAlignOperator.h"

#include <pqCoreUtilities.h>

#include <QDebug>

namespace tomviz
{

AddAlignReaction::AddAlignReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

AddAlignReaction::~AddAlignReaction()
{
}

void AddAlignReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != nullptr &&
        ActiveObjects::instance().activeDataSource()->type() == DataSource::TiltSeries);
}

void AddAlignReaction::align(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
  {
    qDebug() << "Exiting early - no data found.";
    return;
  }

  QSharedPointer<Operator> Op(new TranslateAlignOperator(source));
  EditOperatorDialog *dialog = new EditOperatorDialog(Op, source,
                                                      pqCoreUtilities::mainWidget());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle("Manual Image Alignment");
  dialog->show();
}

}
