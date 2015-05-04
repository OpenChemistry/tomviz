/******************************************************************************

  This source file is part of the TEM tomography project.

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
#include "AlignWidget.h"
#include "DataSource.h"
#include "pqCoreUtilities.h"

#include <QDebug>

namespace TEM
{
//-----------------------------------------------------------------------------
AddAlignReaction::AddAlignReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

//-----------------------------------------------------------------------------
AddAlignReaction::~AddAlignReaction()
{
}

//-----------------------------------------------------------------------------
void AddAlignReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != NULL);
}

//-----------------------------------------------------------------------------
void AddAlignReaction::align(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
    {
    qDebug() << "Exiting early - no data :-(";
    return;
    }

  AlignWidget *widget = new AlignWidget(source, pqCoreUtilities::mainWidget(),
                                        Qt::Window);
  widget->setAttribute(Qt::WA_DeleteOnClose);
  widget->show();
  widget->raise();
}

}
