/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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

SnapshotOperator::SnapshotOperator(DataSource* source, QObject* p)
  : Operator(p), m_dataSource(source)
{
  setSupportsCancel(false);
  setHasChildDataSource(true);
  connect(
    this,
    static_cast<void (Operator::*)(const QString&,
                                   vtkSmartPointer<vtkDataObject>)>(
      &Operator::newChildDataSource),
    this,
    [this](const QString& label, vtkSmartPointer<vtkDataObject> childData) {
      this->createNewChildDataSource(label, childData, DataSource::Volume,
                                     DataSource::PersistenceState::Modified);
    });
}

QIcon SnapshotOperator::icon() const
{
  return QIcon(":/icons/pqLock.png");
}

Operator* SnapshotOperator::clone() const
{
  return new SnapshotOperator(m_dataSource);
}

QJsonObject SnapshotOperator::serialize() const
{
  auto json = Operator::serialize();

  if (hasChildDataSource() && childDataSource() != nullptr &&
      childDataSource()->persistenceState() ==
        DataSource::PersistenceState::Saved) {
    json["update"] = false;
  }

  return json;
}

bool SnapshotOperator::deserialize(const QJsonObject& json)
{
  if (json.contains("update")) {
    m_updateCache = json["update"].toBool(false);
  }

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
} // namespace tomviz
