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

#include <QDebug>
#include <QMainWindow>
#include <QAction>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>
#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkRenderer.h>
#include <pqCoreUtilities.h>

#include "ActiveObjects.h"
#include "CropOperator.h"
#include "EditOperatorDialog.h"
#include "DataSource.h"


namespace tomviz
{
//-----------------------------------------------------------------------------
CropReaction::CropReaction(QAction* parentObject, QMainWindow* mw)
  : pqReaction(parentObject), mainWindow(mw)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

//-----------------------------------------------------------------------------
CropReaction::~CropReaction()
{
}

//-----------------------------------------------------------------------------
void CropReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != nullptr);
}

//-----------------------------------------------------------------------------
void CropReaction::crop(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
  {
    qDebug() << "Exiting early - no data :-(";
    return;
  }
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    source->producer()->GetClientSideObject());
  vtkImageData *image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  QSharedPointer<Operator> Op(new CropOperator(image->GetExtent(), image->GetOrigin(),
                                               image->GetSpacing()));

  EditOperatorDialog *dialog = new EditOperatorDialog(Op, source, this->mainWindow);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}
}
