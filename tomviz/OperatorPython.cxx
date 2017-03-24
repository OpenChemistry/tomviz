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
#include "OperatorPython.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QtDebug>

#include "DataSource.h"
#include "EditOperatorWidget.h"
#include "OperatorResult.h"
#include "PythonUtilities.h"
#include "Utilities.h"
#include "pqPythonSyntaxHighlighter.h"

#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

#include "ui_EditPythonOperatorWidget.h"

namespace {

class EditPythonOperatorWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT
  typedef tomviz::EditOperatorWidget Superclass;

public:
  EditPythonOperatorWidget(QWidget* p, tomviz::OperatorPython* o)
    : Superclass(p), Op(o), Ui()
  {
    this->Ui.setupUi(this);
    this->Ui.name->setText(o->label());
    if (!o->script().isEmpty()) {
      this->Ui.script->setPlainText(o->script());
    }
    new pqPythonSyntaxHighlighter(this->Ui.script, this);
  }
  void applyChangesToOperator() override
  {
    if (this->Op) {
      this->Op->setLabel(this->Ui.name->text());
      this->Op->setScript(this->Ui.script->toPlainText());
    }
  }

private:
  QPointer<tomviz::OperatorPython> Op;
  Ui::EditPythonOperatorWidget Ui;
};
}

namespace tomviz {

class OperatorPython::OPInternals
{
public:
  Python::Module OperatorModule;
  Python::Module TransformModule;
  Python::Function TransformMethod;
  Python::Module InternalModule;
  Python::Function FindTransformScalarsFunction;
  Python::Function IsCancelableFunction;
};

OperatorPython::OperatorPython(QObject* parentObject)
  : Superclass(parentObject), Internals(new OperatorPython::OPInternals()),
    Label("Python Operator")
{
  Python::initialize();

  {
    Python python;
    this->Internals->OperatorModule = python.import("tomviz.utils");
    if (!this->Internals->OperatorModule.isValid()) {
      qCritical() << "Failed to import tomviz.utils module.";
    }

    this->Internals->InternalModule = python.import("tomviz._internal");
    if (!this->Internals->InternalModule.isValid()) {
      qCritical() << "Failed to import tomviz._internal module.";
    }

    this->Internals->IsCancelableFunction =
      this->Internals->InternalModule.findFunction("is_cancelable");
    if (!this->Internals->IsCancelableFunction.isValid()) {
      qCritical() << "Unable to locate is_cancelable.";
    }

    this->Internals->FindTransformScalarsFunction =
      this->Internals->InternalModule.findFunction("find_transform_scalars");
    if (!this->Internals->FindTransformScalarsFunction.isValid()) {
      qCritical() << "Unable to locate find_transform_scalars.";
    }
  }
  // This connection is needed so we can create new child data sources in the UI
  // thread from a pipeline worker threads.
  connect(this, SIGNAL(newChildDataSource(const QString&,
                                          vtkSmartPointer<vtkDataObject>)),
          this, SLOT(createNewChildDataSource(const QString&,
                                              vtkSmartPointer<vtkDataObject>)));
  connect(
    this,
    SIGNAL(newOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>)),
    this,
    SLOT(setOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>)));
}

OperatorPython::~OperatorPython()
{
}

void OperatorPython::setLabel(const QString& txt)
{
  this->Label = txt;
  emit labelModified();
}

QIcon OperatorPython::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqProgrammableFilter24.png");
}

void OperatorPython::setJSONDescription(const QString& str)
{
  if (this->jsonDescription == str) {
    return;
  }

  this->jsonDescription = str;

  QJsonDocument document = QJsonDocument::fromJson(jsonDescription.toLatin1());
  if (!document.isObject()) {
    qCritical() << "Failed to parse operator JSON";
    qCritical() << jsonDescription;
    return;
  }

  QJsonObject root = document.object();

  // Get the label for the operator
  QJsonValueRef labelNode = root["label"];
  if (!labelNode.isUndefined() && !labelNode.isNull()) {
    setLabel(labelNode.toString());
  }

  m_resultNames.clear();
  m_childDataSourceNamesAndLabels.clear();

  // Get the number of results
  QJsonValueRef resultsNode = root["results"];
  if (!resultsNode.isUndefined() && !resultsNode.isNull()) {
    QJsonArray resultsArray = resultsNode.toArray();
    QJsonObject::size_type numResults = resultsArray.size();
    setNumberOfResults(numResults);

    for (QJsonObject::size_type i = 0; i < numResults; ++i) {
      OperatorResult* oa = resultAt(i);
      if (!oa) {
        Q_ASSERT(oa != nullptr);
      }
      QJsonObject resultNode = resultsArray[i].toObject();
      QJsonValueRef nameValue = resultNode["name"];
      if (!nameValue.isUndefined() && !nameValue.isNull()) {
        oa->setName(nameValue.toString());
        m_resultNames.append(nameValue.toString());
      }
      QJsonValueRef labelValue = resultNode["label"];
      if (!labelValue.isUndefined() && !labelValue.isNull()) {
        oa->setLabel(labelValue.toString());
      }
    }
  }

  // Get child dataset information
  QJsonValueRef childDatasetNode = root["children"];
  if (!childDatasetNode.isUndefined() && !childDatasetNode.isNull()) {
    QJsonArray childDatasetArray = childDatasetNode.toArray();
    QJsonObject::size_type size = childDatasetArray.size();
    if (size != 1) {
      qCritical() << "Only one child dataset is supported for now. Found"
                  << size << " but only the first will be used";
    }
    if (size > 0) {
      setHasChildDataSource(true);
      QJsonObject dataSourceNode = childDatasetArray[0].toObject();
      QJsonValueRef nameValue = dataSourceNode["name"];
      QJsonValueRef labelValue = dataSourceNode["label"];
      if (!nameValue.isUndefined() && !nameValue.isNull() &&
          !labelValue.isUndefined() && !labelValue.isNull()) {
        QPair<QString, QString> nameLabelPair(QString(nameValue.toString()),
                                              QString(labelValue.toString()));
        m_childDataSourceNamesAndLabels.append(nameLabelPair);
      } else if (nameValue.isNull()) {
        qCritical() << "No name given for child DataSet";
      } else if (labelValue.isNull()) {
        qCritical() << "No label given for child DataSet";
      }
    }
  }
}

const QString& OperatorPython::JSONDescription() const
{
  return this->jsonDescription;
}

void OperatorPython::setScript(const QString& str)
{
  if (this->Script != str) {
    this->Script = str;

    Python::Object result;
    {
      Python python;
      QString moduleName = QString("tomviz_%1").arg(this->label());
      this->Internals->TransformModule =
        python.import(this->Script, this->label(), moduleName);
      if (!this->Internals->TransformModule.isValid()) {
        qCritical("Failed to create module.");
        return;
      }

      // Create capsule to hold the pointer to the operator in the python world
      Python::Tuple findArgs(2);
      Python::Capsule op(this);

      findArgs.set(0, this->Internals->TransformModule);
      findArgs.set(1, op);

      this->Internals->TransformMethod =
        this->Internals->FindTransformScalarsFunction.call(findArgs);
      if (!this->Internals->TransformMethod.isValid()) {
        qCritical("Script doesn't have any 'transform_scalars' function.");
        return;
      }

      Python::Tuple isArgs(1);
      isArgs.set(0, this->Internals->TransformModule);

      result = this->Internals->IsCancelableFunction.call(isArgs);
      if (!result.isValid()) {
        qCritical("Error calling is_cancelable.");
        return;
      }
    }

    this->setSupportsCancel(result.toBool());

    emit this->transformModified();
  }
}

bool OperatorPython::applyTransform(vtkDataObject* data)
{
  if (this->Script.isEmpty()) {
    return false;
  }
  if (!this->Internals->OperatorModule.isValid() ||
      !this->Internals->TransformMethod.isValid()) {
    return false;
  }

  Q_ASSERT(data);

  Python::Object pydata = Python::VTK::GetObjectFromPointer(data);

  Python::Object result;
  {
    Python python;

    Python::Tuple args(1);
    args.set(0, pydata);

    Python::Dict kwargs;
    foreach (QString key, m_arguments.keys()) {
      Variant value = toVariant(m_arguments[key]);
      kwargs.set(key, value);
    }

    result = this->Internals->TransformMethod.call(args, kwargs);
    if (!result.isValid()) {
      qCritical("Failed to execute the script.");
      return false;
    }
  }

  // Look for additional outputs from the filter returned in a dictionary
  int check = 0;
  {
    Python python;
    check = result.isDict();
  }

  bool errorEncountered = false;
  if (check) {
    Python python;
    Python::Dict outputDict = result.toDict();
    // Results (tables, etc.)
    for (int i = 0; i < m_resultNames.size(); ++i) {
      Python::Object pyDataObject;
      pyDataObject = outputDict[m_resultNames[i]];

      if (!pyDataObject.isValid()) {
        errorEncountered = true;
        qCritical() << "No result named" << m_resultNames[i]
                    << "defined in output dictionary.\n";
        continue;
      }
      vtkObjectBase* vtkobject =
        Python::VTK::GetPointerFromObject(pyDataObject, "vtkDataObject");
      vtkDataObject* dataObject = vtkDataObject::SafeDownCast(vtkobject);
      if (dataObject) {
        // Emit signal so we switch back to UI thread
        emit newOperatorResult(m_resultNames[i], dataObject);
      } else {
        qCritical() << "Result named '" << m_resultNames[i]
                    << "' is not a vtkDataObject";
        continue;
      }
    }

    // Segmentations, reconstructions, etc.
    for (int i = 0; i < m_childDataSourceNamesAndLabels.size(); ++i) {
      QPair<QString, QString> nameLabelPair =
        m_childDataSourceNamesAndLabels[i];
      QString name(nameLabelPair.first);
      QString label(nameLabelPair.second);
      Python::Object child = outputDict[name];

      if (!child.isValid()) {
        errorEncountered = true;
        qCritical() << "No child data source named '" << name
                    << "' defined in output dictionary.\n";
        continue;
      }

      vtkObjectBase* vtkobject =
        Python::VTK::GetPointerFromObject(child, "vtkDataObject");
      vtkSmartPointer<vtkDataObject> childData =
        vtkDataObject::SafeDownCast(vtkobject);
      if (childData) {
        emit newChildDataSource(label, childData);
      }
    }

    if (errorEncountered) {

      qCritical() << "Dictionary return from Python script is:\n"
                  << outputDict.toString();
    }
  }

  return !errorEncountered;
}

Operator* OperatorPython::clone() const
{
  OperatorPython* newClone = new OperatorPython();
  newClone->setLabel(this->label());
  newClone->setScript(this->script());
  newClone->setJSONDescription(this->JSONDescription());
  return newClone;
}

bool OperatorPython::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("json_description")
    .set_value(this->JSONDescription().toLatin1().data());
  ns.append_attribute("label").set_value(this->label().toLatin1().data());
  ns.append_attribute("script").set_value(this->script().toLatin1().data());
  pugi::xml_node argsNode = ns.append_child("arguments");
  return tomviz::serialize(m_arguments, argsNode);
}

bool OperatorPython::deserialize(const pugi::xml_node& ns)
{
  this->setJSONDescription(ns.attribute("json_description").as_string());
  this->setLabel(ns.attribute("label").as_string());
  this->setScript(ns.attribute("script").as_string());
  m_arguments.clear();
  return tomviz::deserialize(m_arguments, ns.child("arguments"));
}

EditOperatorWidget* OperatorPython::getEditorContents(QWidget* p)
{
  return new EditPythonOperatorWidget(p, this);
}

void OperatorPython::createNewChildDataSource(
  const QString& label, vtkSmartPointer<vtkDataObject> childData)
{

  vtkSMProxyManager* proxyManager = vtkSMProxyManager::GetProxyManager();
  vtkSMSessionProxyManager* sessionProxyManager =
    proxyManager->GetActiveSessionProxyManager();

  pqSMProxy producerProxy;
  producerProxy.TakeReference(
    sessionProxyManager->NewProxy("sources", "TrivialProducer"));
  producerProxy->UpdateVTKObjects();

  vtkTrivialProducer* producer =
    vtkTrivialProducer::SafeDownCast(producerProxy->GetClientSideObject());
  if (!producer) {
    qWarning() << "Could not get TrivialProducer from proxy";
    return;
  }

  producer->SetOutput(childData);

  DataSource* childDS = new DataSource(
    vtkSMSourceProxy::SafeDownCast(producerProxy), DataSource::Volume, this,
      DataSource::PersistenceState::Transient);

  childDS->setFilename(label.toLatin1().data());
  this->setChildDataSource(childDS);
  emit Operator::newChildDataSource(childDS);
}

void OperatorPython::setOperatorResult(const QString& name,
                                       vtkSmartPointer<vtkDataObject> result)
{
  bool resultWasSet = this->setResult(name.toLatin1().data(), result);
  if (!resultWasSet) {
    qCritical() << "Could not set result '" << name << "'";
  }
}

void OperatorPython::setArguments(QMap<QString, QVariant> args)
{
  m_arguments = args;
}

QMap<QString, QVariant> OperatorPython::arguments() const
{
  return m_arguments;
}
}
#include "OperatorPython.moc"
