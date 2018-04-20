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
#include "Operator.h"

#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "ModuleManager.h"
#include "OperatorResult.h"

#include "vtkImageData.h"
#include "vtkSMSourceProxy.h"

#include <QJsonArray>
#include <QList>
#include <QTimer>

#include <QDebug>

namespace tomviz {

Operator::Operator(QObject* parentObject) : QObject(parentObject)
{
  qRegisterMetaType<TransformResult>("TransformResult");
  qRegisterMetaType<vtkSmartPointer<vtkDataObject>>();
}

Operator::~Operator()
{
  setNumberOfResults(0);

  emit aboutToBeDestroyed(this);

  if (hasChildDataSource()) {
    auto cds = childDataSource();
    // If the operator failed, the child data source will be null
    if (cds) {
      cds->removeAllOperators();
    }
  }
}

DataSource* Operator::dataSource()
{
  return qobject_cast<DataSource*>(parent());
}

TransformResult Operator::transform(vtkDataObject* data)
{
  m_state = OperatorState::Running;
  emit transformingStarted();
  setProgressStep(0);
  bool result = this->applyTransform(data);
  TransformResult transformResult =
    result ? TransformResult::Complete : TransformResult::Error;
  // If the user requested the operator to be canceled then when it returns
  // we should treat is as canceled.
  if (isCanceled()) {
    transformResult = TransformResult::Canceled;
  } else {
    m_state = static_cast<OperatorState>(transformResult);
  }
  emit transformingDone(transformResult);

  return transformResult;
}

void Operator::setNumberOfResults(int n)
{
  int previousSize = m_results.size();
  if (previousSize < n) {
    // Resize the list
    m_results.reserve(n);
    for (int i = previousSize; i < n; ++i) {
      auto oa = new OperatorResult(this);
      m_results.append(oa);
    }
  } else {
    for (int i = n; i < previousSize; ++i) {
      auto result = m_results.takeLast();
      delete result;
    }
  }
}

int Operator::numberOfResults() const
{
  return m_results.size();
}

bool Operator::setResult(int index, vtkDataObject* object)
{
  if (index < 0 || index >= numberOfResults()) {
    return false;
  }

  auto result = m_results[index];
  Q_ASSERT(result);
  result->setDataObject(object);
  m_results[index] = result;

  return true;
}

bool Operator::setResult(const char* name, vtkDataObject* object)
{
  bool valueSet = false;
  QString qname(name);
  foreach (auto result, m_results) {
    if (result->name() == qname) {
      result->setDataObject(object);
      valueSet = true;
      break;
    }
  }

  return valueSet;
}

OperatorResult* Operator::resultAt(int i) const
{
  if (i < 0 || i >= m_results.size()) {
    return nullptr;
  }
  return m_results[i];
}

void Operator::setHasChildDataSource(bool value)
{
  m_hasChildDataSource = value;
}

bool Operator::hasChildDataSource() const
{
  return m_hasChildDataSource;
}

void Operator::setChildDataSource(DataSource* source)
{
  ModuleManager::instance().addChildDataSource(source);
  m_childDataSource = source;
}

DataSource* Operator::childDataSource() const
{
  return m_childDataSource;
}

QJsonObject Operator::serialize() const
{
  QJsonObject json;
  if (childDataSource()) {
    QJsonArray dataSources;
    DataSource* ds = childDataSource();
    dataSources.append(ds->serialize());
    json["dataSources"] = dataSources;
  }
  return json;
}

bool Operator::deserialize(const QJsonObject& json)
{
  if (json.contains("dataSources")) {
    // This means that this operator is the end of the line, and needs to
    // restore the child once it has finished doing its thing.
    qDebug() << "We need to do something with this:" << json["dataSources"];
  }
  return true;
}

EditOperatorDialog* Operator::customDialog() const
{
  return m_customDialog;
}

void Operator::setCustomDialog(EditOperatorDialog* dialog)
{
  if (m_customDialog) {
    std::cerr << "Error: Attempting to set custom dialog on an operator that "
                 "already has one!";
    return;
  }

  m_customDialog = dialog;
}

void Operator::createNewChildDataSource(
  const QString& label, vtkSmartPointer<vtkDataObject> childData,
  DataSource::DataSourceType type, DataSource::PersistenceState state)
{
  if (this->childDataSource() == nullptr) {
    DataSource* childDS =
      new DataSource(vtkImageData::SafeDownCast(childData), type, this, state);
    childDS->setLabel(label);
    setChildDataSource(childDS);
    setHasChildDataSource(true);
    emit Operator::newChildDataSource(childDS);
  }
  // Reuse the existing "Output" data source.
  else {
    childDataSource()->setLabel(label);
    childDataSource()->setForkable(true);
    setHasChildDataSource(true);
  }
}
}
