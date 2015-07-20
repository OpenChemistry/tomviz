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
#include <QVBoxLayout>
#include <QDialog>
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
#include "CropDialog.h"
#include "CropWidget.h"
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
        ActiveObjects::instance().activeDataSource() != NULL &&
        ActiveObjects::instance().activeDataSource()->type() == DataSource::Volume);
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

  CropDialog* dialog = new CropDialog(this->mainWindow, source);
  vtkSMViewProxy *viewProxy = ActiveObjects::instance().activeView();
  CropWidget *w = new CropWidget(source, viewProxy->GetRenderWindow()->GetInteractor(), dialog);

  this->connect(w, SIGNAL(bounds(double*)),
                           dialog, SLOT(updateBounds(double*)));
  this->connect(dialog, SIGNAL(bounds(int*)),
                w, SLOT(updateBounds(int*)));

  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}
}
