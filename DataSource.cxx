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

#include "OperatorPython.h"
#include "Utilities.h"
#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkPVArrayInformation.h"
#include "vtkSmartPointer.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTransferFunctionManager.h"
#include "vtkSMTransferFunctionProxy.h"
#include "vtkTrivialProducer.h"

#include <vtk_pugixml.h>

namespace TEM
{

class DataSource::DSInternals
{
public:
  vtkSmartPointer<vtkSMSourceProxy> OriginalDataSource;
  vtkWeakPointer<vtkSMSourceProxy> Producer;
  QList<QSharedPointer<Operator> > Operators;
  vtkSmartPointer<vtkSMProxy> ColorMap;
};

//-----------------------------------------------------------------------------
DataSource::DataSource(vtkSMSourceProxy* dataSource, QObject* parentObject)
  : Superclass(parentObject),
  Internals(new DataSource::DSInternals())
{
  Q_ASSERT(dataSource);
  this->Internals->OriginalDataSource = dataSource;

  vtkNew<vtkSMParaViewPipelineController> controller;
  vtkSMSessionProxyManager* pxm = dataSource->GetSessionProxyManager();
  Q_ASSERT(pxm);

  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  Q_ASSERT(source != NULL);
  Q_ASSERT(vtkSMSourceProxy::SafeDownCast(source));

  // We add an annotation to the proxy so that it'll be easier for code to
  // locate registered pipeline proxies that are being treated as data sources.
  TEM::annotateDataProducer(source,
    vtkSMPropertyHelper(dataSource,
      vtkSMCoreUtilities::GetFileNameProperty(dataSource)).GetAsString());
  controller->RegisterPipelineProxy(source);
  this->Internals->Producer = vtkSMSourceProxy::SafeDownCast(source);

  // Setup color map for this data-source.
  static unsigned int colorMapCounter=0;
  colorMapCounter++;

  vtkNew<vtkSMTransferFunctionManager> tfmgr;
  this->Internals->ColorMap = tfmgr->GetColorTransferFunction(
    QString("DataSourceColorMap%1").arg(colorMapCounter).toLatin1().data(), pxm);

  // every time the data changes, we should update the color map.
  this->connect(this, SIGNAL(dataChanged()), SLOT(updateColorMap()));

  this->resetData();
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
QString DataSource::filename() const
{
  vtkSMProxy* dataSource = this->originalDataSource();
  return vtkSMPropertyHelper(dataSource,
    vtkSMCoreUtilities::GetFileNameProperty(dataSource)).GetAsString();
}

//-----------------------------------------------------------------------------
bool DataSource::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("number_of_operators").set_value(
    static_cast<int>(this->Internals->Operators.size()));

  foreach (QSharedPointer<Operator> op, this->Internals->Operators)
    {
    pugi::xml_node node = ns.append_child("Operator");
    if (!op->serialize(node))
      {
      qWarning("failed to serialize Operator. Skipping it.");
      ns.remove_child(node);
      }
    }
  return true;
}

//-----------------------------------------------------------------------------
bool DataSource::deserialize(const pugi::xml_node& ns)
{
  int num_operators = ns.attribute("number_of_operators").as_int(-1);
  if (num_operators < 0)
    {
    return false;
    }

  this->Internals->Operators.clear();
  this->resetData();

  for (pugi::xml_node node=ns.child("Operator"); node; node = node.next_sibling("Operator"))
    {
    QSharedPointer<OperatorPython> op (new OperatorPython());
    if (op->deserialize(node))
      {
      this->addOperator(op);
      }
    }
  return true;
}

//-----------------------------------------------------------------------------
DataSource* DataSource::clone(bool clone_operators) const
{
  DataSource* newClone = new DataSource(this->Internals->OriginalDataSource);
  if (clone_operators)
    {
    // now, clone the operators.
    foreach (QSharedPointer<Operator> op, this->Internals->Operators)
      {
      QSharedPointer<Operator> opClone(op->clone());
      newClone->addOperator(opClone);
      }
    }
  return newClone;
}

//-----------------------------------------------------------------------------
vtkSMSourceProxy* DataSource::originalDataSource() const
{
  return this->Internals->OriginalDataSource;
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
  this->connect(op.data(), SIGNAL(transformModified()),
    SLOT(operatorTransformModified()));
  emit this->operatorAdded(op.data());
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
    this->Internals->Producer->UpdatePipeline();
    }

  emit this->dataChanged();
}

//-----------------------------------------------------------------------------
const QList<QSharedPointer<Operator> >& DataSource::operators() const
{
  return this->Internals->Operators;
}

//-----------------------------------------------------------------------------
void DataSource::resetData()
{
  vtkSMSourceProxy* dataSource = this->Internals->OriginalDataSource;
  Q_ASSERT(dataSource);

  dataSource->UpdatePipeline();
  vtkAlgorithm* vtkalgorithm = vtkAlgorithm::SafeDownCast(
    dataSource->GetClientSideObject());
  Q_ASSERT(vtkalgorithm);

  vtkSMSourceProxy* source = this->Internals->Producer;
  Q_ASSERT(source != NULL);

  // Create a clone and release the reader data.
  vtkDataObject* data = vtkalgorithm->GetOutputDataObject(0);
  vtkDataObject* clone = data->NewInstance();
  clone->DeepCopy(data);
  //data->ReleaseData();  FIXME: how it this supposed to work? I get errors on
  //attempting to re-execute the reader pipeline in clone().

  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    source->GetClientSideObject());
  Q_ASSERT(tp);
  tp->SetOutput(clone);
  clone->FastDelete();
  emit this->dataChanged();
}

//-----------------------------------------------------------------------------
void DataSource::operatorTransformModified()
{
  bool prev = this->blockSignals(true);

  this->resetData();
  foreach (QSharedPointer<Operator> op, this->Internals->Operators)
    {
    this->operate(op.data());
    }
  this->blockSignals(prev);
  emit this->dataChanged();
}

//-----------------------------------------------------------------------------
vtkSMProxy* DataSource::colorMap() const
{
  return this->Internals->ColorMap;
}

//-----------------------------------------------------------------------------
vtkSMProxy* DataSource::opacityMap() const
{
  return this->Internals->ColorMap?
  vtkSMPropertyHelper(this->Internals->ColorMap, "ScalarOpacityFunction").GetAsProxy() : NULL;
}

//-----------------------------------------------------------------------------
void DataSource::updateColorMap()
{
  // rescale the color/opacity maps for the data source.
  vtkSMProxy* cmap = this->Internals->ColorMap;
  vtkSMProxy* omap = vtkSMPropertyHelper(cmap, "ScalarOpacityFunction").GetAsProxy();
  vtkPVArrayInformation* ainfo = TEM::scalarArrayInformation(this->producer());
  if (ainfo != NULL && vtkSMPropertyHelper(cmap, "LockScalarRange").GetAsInt() == 0)
    {
    // assuming single component arrays.
    Q_ASSERT(ainfo->GetNumberOfComponents() == 1);
    vtkSMTransferFunctionProxy::RescaleTransferFunction(cmap, ainfo->GetComponentRange(0));
    vtkSMTransferFunctionProxy::RescaleTransferFunction(omap, ainfo->GetComponentRange(0));
    }
}

}
