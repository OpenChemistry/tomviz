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
#include "ReconstructionReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "pqCoreUtilities.h"
#include <vtkTrivialProducer.h>
#include <vtkSMSourceProxy.h>

#include "ReconstructionWidget.h"
#include "TomographyReconstruction.h"

#include <QDebug>
#include <QDialog>
#include <QHBoxLayout>

namespace tomviz
{

ReconstructionReaction::ReconstructionReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

ReconstructionReaction::~ReconstructionReaction()
{
}

void ReconstructionReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != NULL &&
        ActiveObjects::instance().activeDataSource()->type() == DataSource::TiltSeries);
}

void ReconstructionReaction::recon(DataSource* input)
{
  input = input ? input : ActiveObjects::instance().activeDataSource();
  if (!input)
  {
    qDebug() << "Exiting early - no data :-(";
    return;
  }

  QDialog d;
  d.setLayout(new QHBoxLayout);
  ReconstructionWidget *reconWidget = new ReconstructionWidget(input, &d);
  d.layout()->addWidget(reconWidget);
  d.connect(reconWidget, SIGNAL(reconstructionCancelled()), SLOT(reject()));
  d.connect(reconWidget, SIGNAL(reconstructionFinished()), SLOT(accept()));
  d.exec();

}

}
