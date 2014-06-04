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
#include "DataSource.h"

#include "Operator.h"
#include "Utilities.h"
#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

namespace TEM
{

class DataSource::DSInternals
{
public:
  vtkSmartPointer<vtkSMSourceProxy> OriginalDataSource;
  vtkWeakPointer<vtkSMSourceProxy> Producer;
  QList<QSharedPointer<Operator> > Operators;
};

//-----------------------------------------------------------------------------
DataSource::DataSource(vtkSMSourceProxy* dataSource, QObject* parentObject)
  : Superclass(parentObject),
  Internals(new DataSource::DSInternals())
{
  Q_ASSERT(dataSource);

  this->Internals->OriginalDataSource = dataSource;

  dataSource->UpdatePipeline();
  vtkAlgorithm* vtkalgorithm = vtkAlgorithm::SafeDownCast(
    dataSource->GetClientSideObject());
  Q_ASSERT(vtkalgorithm);

  vtkNew<vtkSMParaViewPipelineController> controller;
  vtkSMSessionProxyManager* pxm = dataSource->GetSessionProxyManager();
  Q_ASSERT(pxm);

  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  Q_ASSERT(source != NULL);
  Q_ASSERT(vtkSMSourceProxy::SafeDownCast(source));

  // Create a clone and release the reader data.
  vtkDataObject* data = vtkalgorithm->GetOutputDataObject(0);
  vtkDataObject* clone = data->NewInstance();
  clone->ShallowCopy(data);
  //data->ReleaseData();  FIXME: how it this supposed to work? I get errors on
  //attempting to re-execute the reader pipeline in clone().

  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    source->GetClientSideObject());
  Q_ASSERT(tp);
  tp->SetOutput(clone);
  clone->FastDelete();

  // We add an annotation to the proxy so that it'll be easier for code to
  // locate registered pipeline proxies that are being treated as data sources.
  TEM::annotateDataProducer(source,
    vtkSMPropertyHelper(dataSource,
      vtkSMCoreUtilities::GetFileNameProperty(dataSource)).GetAsString());
  controller->RegisterPipelineProxy(source);

  this->Internals->Producer = vtkSMSourceProxy::SafeDownCast(source);
}

//-----------------------------------------------------------------------------
DataSource::~DataSource()
{
  if (this->Internals->Producer)
    {
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(this->Internals->Producer);
    }
}

//-----------------------------------------------------------------------------
DataSource* DataSource::clone() const
{
 // vtkAlgorithm::SafeDownCast(this->Internals->OriginalDataSource->GetClientSideObject())->Modified();
  DataSource* newClone = new DataSource(this->Internals->OriginalDataSource);
  // now, clone the operators.
  foreach (QSharedPointer<Operator> op, this->Internals->Operators)
    {
    newClone->addOperator(op);
    }
  return newClone;
}


//-----------------------------------------------------------------------------
vtkSMSourceProxy* DataSource::producer() const
{
  return this->Internals->Producer;
}
//-----------------------------------------------------------------------------
int DataSource::addOperator(const QSharedPointer<Operator>& op)
{
  int index = this->Internals->Operators.count();
  this->Internals->Operators.push_back(op);
  this->operate(op.data());
  return index;
}

//-----------------------------------------------------------------------------
void DataSource::operate(Operator* op)
{
  Q_ASSERT(op);

  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    this->Internals->Producer->GetClientSideObject());
  Q_ASSERT(tp);
  if (op->transform(tp->GetOutputDataObject(0)))
    {
    tp->Modified();
    tp->GetOutputDataObject(0)->Modified();
    this->Internals->Producer->MarkModified(NULL);
    }
}

}
