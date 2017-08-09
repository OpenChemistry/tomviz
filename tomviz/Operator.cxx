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
#include "ModuleManager.h"
#include "OperatorResult.h"

#include "vtkSMSourceProxy.h"

#include <QList>
#include <QTimer>

namespace tomviz {

using pugi::xml_attribute;
using pugi::xml_node;

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
    childDataSource()->removeAllOperators();
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

bool Operator::serialize(pugi::xml_node& ns) const
{
  if (hasChildDataSource()) {
    DataSource* ds = childDataSource();
    ns.append_attribute("childDataSource")
      .set_value(ds->producer()->GetGlobalIDAsString());
  }

  return true;
}

bool Operator::deserialize(const pugi::xml_node& ns)
{
  xml_attribute child = ns.attribute("childDataSource");
  if (child) {
    vtkTypeUInt32 id = child.as_int();
    DataSource* childDataSource =
      ModuleManager::instance().lookupDataSource(id);
    setChildDataSource(childDataSource);

    // The operator is not added at this point so the signal is lost, so we need
    // to use a singleShot to emit on the next event loop.
    QTimer::singleShot(0, [this, childDataSource]() {
      emit this->newChildDataSource(childDataSource);
    });
  }

  return true;
}
}
