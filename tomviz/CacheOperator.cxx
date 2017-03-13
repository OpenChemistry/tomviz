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

#include "CacheOperator.h"

#include "DataSource.h"

#include "pqSMProxy.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

#include <QDebug>

namespace tomviz {
CacheOperator::CacheOperator(DataSource* source, QObject* p)
  : Operator(p), m_dataSource(source)
{
  auto t =
    vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
  auto imageData =
    vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  int dataExtent[6];
  imageData->GetExtent(dataExtent);
  for (int i = 0; i < 6; ++i) {
    m_extent[i] = dataExtent[i];
  }
  setSupportsCancel(false);
  setNumberOfResults(1);
  setHasChildDataSource(true);
  connect(this, &CacheOperator::newChildDataSource,
          this, &CacheOperator::createNewChildDataSource);
  connect(this, &CacheOperator::newOperatorResult,
          this, &CacheOperator::setOperatorResult);
}

QIcon CacheOperator::icon() const
{
  return QIcon(":/icons/pqLock.png");
}

Operator* CacheOperator::clone() const
{
  return new CacheOperator(m_dataSource);
}

bool CacheOperator::serialize(pugi::xml_node&) const
{
  // No state to serialize yet
  return true;
}

bool CacheOperator::deserialize(const pugi::xml_node&)
{
  // No state to serialize yet
  return true;
}

QWidget* CacheOperator::getCustomProgressWidget(QWidget*) const
{
  return nullptr;
}

bool CacheOperator::applyTransform(vtkDataObject* dataObject)
{
  if (!m_updateCache) {
    // We already ran once, now mark as successful and leave child data alone.
    return true;
  }

  m_updateCache = false;
  vtkImageData* imageData = vtkImageData::SafeDownCast(dataObject);
  if (!imageData) {
    return false;
  }
  int dataExtent[6];
  imageData->GetExtent(dataExtent);
  for (int i = 0; i < 6; ++i) {
    if (dataExtent[i] != m_extent[i]) {
      // Extent changing shouldn't matter, but update so that correct
      // number of steps can be reported.
      m_extent[i] = dataExtent[i];
    }
  }

  vtkNew<vtkImageData> cacheImage;
  cacheImage->DeepCopy(imageData);

  emit newOperatorResult(cacheImage.Get());
  emit newChildDataSource("Cache", cacheImage.Get());
  return true;
}

void CacheOperator::createNewChildDataSource(
  const QString& label, vtkSmartPointer<vtkDataObject> childData)
{
  auto proxyManager = vtkSMProxyManager::GetProxyManager();
  auto sessionProxyManager =
    proxyManager->GetActiveSessionProxyManager();

  pqSMProxy producerProxy;
  producerProxy.TakeReference(
    sessionProxyManager->NewProxy("sources", "TrivialProducer"));
  producerProxy->UpdateVTKObjects();

  auto producer =
    vtkTrivialProducer::SafeDownCast(producerProxy->GetClientSideObject());
  if (!producer) {
    qWarning() << "Could not get TrivialProducer from proxy";
    return;
  }

  producer->SetOutput(childData);

  auto childDS = new DataSource(
    vtkSMSourceProxy::SafeDownCast(producerProxy), DataSource::Volume, this);

  childDS->setFilename(label.toLatin1().data());
  setChildDataSource(childDS);
}

void CacheOperator::setOperatorResult(vtkSmartPointer<vtkDataObject> result)
{
  bool resultWasSet = setResult(0, result);
  if (!resultWasSet) {
    qCritical() << "Could not set result 0";
  }
}
}
