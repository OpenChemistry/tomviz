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
#include "OperatorResult.h"

#include <QList>

namespace tomviz {

Operator::Operator(QObject* parentObject) : QObject(parentObject)
{
}

Operator::~Operator()
{
  setNumberOfResults(0);
}

DataSource* Operator::dataSource()
{
  return qobject_cast<DataSource*>(parent());
}

bool Operator::transform(vtkDataObject* data)
{
  this->m_state = OperatorState::RUNNING;
  emit transformingStarted();
  emit updateProgress(0);
  bool result = this->applyTransform(data);
  this->m_state = result ? OperatorState::COMPLETE : OperatorState::ERROR;
  emit transformingDone(result);
  return result;
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
  m_childDataSource = source;
}

DataSource* Operator::childDataSource() const
{
  return m_childDataSource;
}
}
