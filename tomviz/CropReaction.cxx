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
#include <QDebug>
#include <QMainWindow>
#include <pqCoreUtilities.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>

#include "ActiveObjects.h"
#include "CropOperator.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"

namespace tomviz {

CropReaction::CropReaction(QAction* parentObject, QMainWindow* mw)
  : pqReaction(parentObject), mainWindow(mw)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

CropReaction::~CropReaction()
{
}

void CropReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

void CropReaction::crop(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source) {
    qDebug() << "Exiting early - no data :-(";
    return;
  }

  Operator* Op = new CropOperator();

  EditOperatorDialog* dialog =
    new EditOperatorDialog(Op, source, true, this->mainWindow);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}
}
