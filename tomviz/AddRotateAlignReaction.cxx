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
#include "AddRotateAlignReaction.h"

#include "ActiveObjects.h"
#include "RotateAlignWidget.h"
#include "DataSource.h"
#include <pqCoreUtilities.h>

#include <QDebug>
#include <QDialog>
#include <QHBoxLayout>

namespace tomviz
{

AddRotateAlignReaction::AddRotateAlignReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

AddRotateAlignReaction::~AddRotateAlignReaction()
{
}

void AddRotateAlignReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != NULL &&
        ActiveObjects::instance().activeDataSource()->type() == DataSource::TiltSeries);
}

void AddRotateAlignReaction::align(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
  {
    qDebug() << "Exiting early - no data :-(";
    return;
  }

  QDialog *dialog = new QDialog(pqCoreUtilities::mainWidget());
  dialog->setWindowTitle("Determine Axis of Rotation");
  RotateAlignWidget *widget = new RotateAlignWidget(source, dialog);
  QHBoxLayout *layout = new QHBoxLayout();
  layout->addWidget(widget);
  dialog->setLayout(layout);

  QObject::connect(widget, SIGNAL(creatingAlignedData()), dialog,
                   SLOT(accept()));
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  dialog->raise();
}

}
