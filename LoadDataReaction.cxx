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
#include "LoadDataReaction.h"

#include "ActiveObjects.h"
#include "pqLoadDataReaction.h"
#include "pqPipelineSource.h"
#include "Utilities.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkTrivialProducer.h"

namespace TEM
{
//-----------------------------------------------------------------------------
LoadDataReaction::LoadDataReaction(QAction* parentObject)
  : Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
LoadDataReaction::~LoadDataReaction()
{
}

//-----------------------------------------------------------------------------
void LoadDataReaction::onTriggered()
{
  vtkNew<vtkSMParaViewPipelineController> controller;

  // Let ParaView deal with reading the data. If we need more customization, we
  // can show our own "File/Open" dialog and then create the appropriate reader
  // proxies.

  QList<pqPipelineSource*> readers = pqLoadDataReaction::loadData();

  QList<pqPipelineSource*> dataSources;

  // Since we want to ditch the reader pipeline and just keep the raw data
  // around. We do this magic.
  foreach (pqPipelineSource* reader, readers)
    {
    pqPipelineSource* dataSource = this->createDataSource(reader);
    Q_ASSERT(dataSource);
    dataSources.push_back(dataSource);

    controller->UnRegisterProxy(reader->getProxy());
    // reader is dangling at this point.
    }
  readers.clear();

  // TODO: do whatever we need to do with all the data sources.

  // make the last one active, I suppose.
  if (dataSources.size() > 0)
    {
    ActiveObjects::instance().setActiveDataSource(dataSources.last());
    }
}

//-----------------------------------------------------------------------------
pqPipelineSource* LoadDataReaction::createDataSource(pqPipelineSource* reader)
{
  Q_ASSERT(reader);

  // update the reader.
  reader->updatePipeline();

  vtkSMProxy* readerProxy = TEM::convert(reader);
  vtkAlgorithm* vtkalgorithm = vtkAlgorithm::SafeDownCast(
    readerProxy->GetClientSideObject());
  Q_ASSERT(vtkalgorithm);

  vtkNew<vtkSMParaViewPipelineController> controller;
  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  Q_ASSERT(source != NULL);

  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    source->GetClientSideObject());
  Q_ASSERT(tp);
  tp->SetOutput(vtkalgorithm->GetOutputDataObject(0));

  // We add an annotation to the proxy so that it'll be easier for code to
  // locate registered pipeline proxies that are being treated as data sources.
  TEM::annotateDataProducer(source,
    vtkSMPropertyHelper(readerProxy,
      vtkSMCoreUtilities::GetFileNameProperty(readerProxy)).GetAsString());

  controller->RegisterPipelineProxy(source);
  return TEM::convert<pqPipelineSource*>(source);
}

//-----------------------------------------------------------------------------
} // end of namespace TEM
