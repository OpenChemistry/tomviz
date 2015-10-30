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

#include "TomographyReconstruction.h"

#include <QDebug>

namespace tomviz
{
//-----------------------------------------------------------------------------
ReconstructionReaction::ReconstructionReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}
    
//-----------------------------------------------------------------------------
ReconstructionReaction::~ReconstructionReaction()
{
}
    
//-----------------------------------------------------------------------------
void ReconstructionReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != NULL &&
        ActiveObjects::instance().activeDataSource()->type() == DataSource::TiltSeries);
}
    
//-----------------------------------------------------------------------------
void ReconstructionReaction::recon(DataSource* input)
{
  input = input ? input : ActiveObjects::instance().activeDataSource();
  if (!input)
  {
    qDebug() << "Exiting early - no data :-(";
    return;
  }
  
  //Get vtkImageData pointer for input DataSource
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    input->producer()->GetClientSideObject());
  vtkImageData *tiltSeries = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  //Create output DataSource
  DataSource* output = input->clone(true,true);
  QString name = output->producer()->GetAnnotation("tomviz.Label");
  name = "Recon_WBP_" + name;
  output->producer()->SetAnnotation("tomviz.Label", name.toAscii().data());
  t = vtkTrivialProducer::SafeDownCast(output->producer()->GetClientSideObject());
  vtkImageData *recon = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  TomographyReconstruction::weightedBackProjection3(tiltSeries,recon);

  output->dataModified();
  // Add the new DataSource
  LoadDataReaction::dataSourceAdded(output);
  
}

}
