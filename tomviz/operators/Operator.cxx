/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Operator.h"

#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "ModuleManager.h"
#include "OperatorFactory.h"
#include "OperatorResult.h"
#include "Pipeline.h"

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

  // Whenever we emit transform modified, let's trip the m_modified flag
  connect(this, &Operator::transformModified, this,
          [this]() { m_modified = true; });

  // When the transorm is completed, let's reset m_modified and m_new flags
  connect(this, &Operator::transformingDone, this, [this]() {
    if (m_state == OperatorState::Complete) {
      m_modified = false;
      m_new = false;
    }
  });
}

Operator::~Operator()
{
  setNumberOfResults(0);
  emit aboutToBeDestroyed(this);
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
  json["type"] = OperatorFactory::instance().operatorType(this);
  json["id"] = QString().sprintf("%p", static_cast<const void*>(this));

  return json;
}

bool Operator::deserialize(const QJsonObject& json)
{
  Q_UNUSED(json);

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
      new DataSource(vtkImageData::SafeDownCast(childData), type,
                     this->dataSource()->pipeline(), state);
    childDS->setLabel(label);
    setChildDataSource(childDS);
    setHasChildDataSource(true);
    emit Operator::newChildDataSource(childDS);
  }
  // Reuse the existing "Output" data source.
  else {
    childDataSource()->setData(childData);
    childDataSource()->setLabel(label);
    childDataSource()->setForkable(true);
    childDataSource()->dataModified();
    setHasChildDataSource(true);
  }
}

void Operator::cancelTransform()
{
  m_state = OperatorState::Canceled;
  emit transformCanceled();
}
} // namespace tomviz
