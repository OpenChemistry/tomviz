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

#include "SnapshotOperator.h"

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

using pugi::xml_attribute;
using pugi::xml_node;

SnapshotOperator::SnapshotOperator(DataSource* source, QObject* p)
  : Operator(p), m_dataSource(source)
{
  setSupportsCancel(false);
  setHasChildDataSource(true);
  connect(this, &SnapshotOperator::newChildDataSource, this,
          &SnapshotOperator::createNewChildDataSource);
}

QIcon SnapshotOperator::icon() const
{
  return QIcon(":/icons/pqLock.png");
}

Operator* SnapshotOperator::clone() const
{
  return new SnapshotOperator(m_dataSource);
}

bool SnapshotOperator::serialize(pugi::xml_node& ns) const
{
  Operator::serialize(ns);
  if (hasChildDataSource() &&
      childDataSource()->persistenceState() ==
        DataSource::PersistenceState::Saved) {
    ns.append_attribute("update").set_value(false);
  }

  return true;
}

bool SnapshotOperator::deserialize(const pugi::xml_node& ns)
{
  Operator::deserialize(ns);
  xml_attribute att = ns.attribute("update");
  if (att) {
    m_updateCache = att.as_bool();
  }
  // No state to serialize yet
  return true;
}

QWidget* SnapshotOperator::getCustomProgressWidget(QWidget*) const
{
  return nullptr;
}

bool SnapshotOperator::applyTransform(vtkDataObject* dataObject)
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

  vtkNew<vtkImageData> cacheImage;
  cacheImage->DeepCopy(imageData);

  emit newChildDataSource("Snapshot", cacheImage.Get());
  return true;
}

void SnapshotOperator::createNewChildDataSource(
  const QString& label, vtkSmartPointer<vtkDataObject> childData)
{
  auto proxyManager = vtkSMProxyManager::GetProxyManager();
  auto sessionProxyManager = proxyManager->GetActiveSessionProxyManager();

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

  auto childDS = new DataSource(vtkSMSourceProxy::SafeDownCast(producerProxy),
                                DataSource::Volume, this,
                                DataSource::PersistenceState::Modified);

  childDS->setFileName(label.toLatin1().data());
  setChildDataSource(childDS);

  emit Operator::newChildDataSource(childDS);
}
}
