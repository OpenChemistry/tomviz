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
#include "vtkPython.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QtDebug>

#include "DataSource.h"
#include "EditOperatorWidget.h"
#include "OperatorResult.h"
#include "Utilities.h"
#include "pqPythonSyntaxHighlighter.h"

#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkPython.h"
#include "vtkPythonInterpreter.h"
#include "vtkPythonUtil.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSmartPyObject.h"
#include "vtkTrivialProducer.h"
#include <pybind11/pybind11.h>

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
  vtkSmartPyObject OperatorModule;
  vtkSmartPyObject Code;
  vtkSmartPyObject TransformModule;
  vtkSmartPyObject TransformMethod;
  vtkSmartPyObject InternalModule;
  vtkSmartPyObject FindTransformScalarsFunction;
  vtkSmartPyObject IsCancelableFunction;
};

OperatorPython::OperatorPython(QObject* parentObject)
  : Superclass(parentObject), Internals(new OperatorPython::OPInternals()),
    Label("Python Operator")
{
  qRegisterMetaType<vtkSmartPointer<vtkDataObject>>();
  vtkPythonInterpreter::Initialize();

  {
    vtkPythonScopeGilEnsurer gilEnsurer(true);
    this->Internals->OperatorModule.TakeReference(
      PyImport_ImportModule("tomviz.utils"));
    if (!this->Internals->OperatorModule) {
      qCritical() << "Failed to import tomviz.utils module.";
      checkForPythonError();
    }

    this->Internals->InternalModule.TakeReference(
      PyImport_ImportModule("tomviz._internal"));
    if (!this->Internals->InternalModule) {
      qCritical() << "Failed to import tomviz._internal module.";
      checkForPythonError();
    }

    this->Internals->IsCancelableFunction.TakeReference(
      PyObject_GetAttrString(this->Internals->InternalModule, "is_cancelable"));
    if (!this->Internals->IsCancelableFunction) {
      checkForPythonError();
      qCritical() << "Unable to locate is_cancelable.";
    }

    this->Internals->FindTransformScalarsFunction.TakeReference(
      PyObject_GetAttrString(this->Internals->InternalModule,
                             "find_transform_scalars"));
    if (!this->Internals->FindTransformScalarsFunction) {
      checkForPythonError();
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
      QJsonValueRef nameValue = resultsArray[i].toObject()["name"];
      if (!nameValue.isUndefined() && !nameValue.isNull()) {
        oa->setName(nameValue.toString());
        m_resultNames.append(nameValue.toString());
      }
      QJsonValueRef labelValue = resultsArray[i].toObject()["label"];
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
      QJsonValueRef nameValue = childDatasetArray[0].toObject()["name"];
      QJsonValueRef labelValue = childDatasetArray[0].toObject()["label"];
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
    this->Internals->Code.TakeReference(nullptr);
    this->Internals->TransformModule.TakeReference(nullptr);
    this->Internals->TransformMethod.TakeReference(nullptr);

    vtkSmartPyObject result;
    {
      vtkPythonScopeGilEnsurer gilEnsurer(true);
      this->Internals->Code.TakeReference(Py_CompileString(
        this->Script.toLatin1().data(), this->label().toLatin1().data(),
        Py_file_input /*Py_eval_input*/));
      if (!this->Internals->Code) {
        checkForPythonError();
        qCritical(
          "Invalid script. Please check the traceback message for details");
        return;
      }

      this->Internals->TransformModule.TakeReference(PyImport_ExecCodeModule(
        QString("tomviz_%1").arg(this->label()).toLatin1().data(),
        this->Internals->Code));
      if (!this->Internals->TransformModule) {
        checkForPythonError();
        qCritical("Failed to create module.");
        return;
      }

      // Create capsule to hold the pointer to the operator in the python world
      vtkSmartPyObject args(PyTuple_New(2));
      pybind11::capsule op(this);
      // Increment ref count as the pybind11::capsule destructor will decrement
      // and we need the capsule to stay around. The PyTuple_SET_ITEM will
      // steal the other reference.
      op.inc_ref();
      // Note we use GetAndIncreaseReferenceCount to increment ref count
      // as PyTuple_SET_ITEM will "steal" the reference.
      PyTuple_SET_ITEM(
        args.GetPointer(), 0,
        this->Internals->TransformModule.GetAndIncreaseReferenceCount());
      PyTuple_SET_ITEM(args.GetPointer(), 1, op.ptr());
      this->Internals->TransformMethod.TakeReference(PyObject_Call(
        this->Internals->FindTransformScalarsFunction, args, nullptr));
      if (!this->Internals->TransformMethod) {
        qCritical("Script doesn't have any 'transform_scalars' function.");
        checkForPythonError();
        return;
      }

      args.TakeReference(PyTuple_New(1));
      // Note we use GetAndIncreaseReferenceCount to increment ref count
      // as PyTuple_SET_ITEM will "steal" the reference.
      PyTuple_SET_ITEM(
        args.GetPointer(), 0,
        this->Internals->TransformModule.GetAndIncreaseReferenceCount());
      result.TakeReference(
        PyObject_Call(this->Internals->IsCancelableFunction, args, nullptr));
      if (!result) {
        qCritical("Error calling is_cancelable.");
        checkForPythonError();
        return;
      }
    }

    this->setSupportsCancel(result == Py_True);

    emit this->transformModified();
  }
}

bool OperatorPython::applyTransform(vtkDataObject* data)
{
  if (this->Script.isEmpty()) {
    return true;
  }
  if (!this->Internals->OperatorModule || !this->Internals->TransformMethod) {
    return true;
  }

  Q_ASSERT(data);

  vtkSmartPyObject pydata(vtkPythonUtil::GetObjectFromPointer(data));

  vtkSmartPyObject result;
  {
    vtkPythonScopeGilEnsurer gilEnsurer(true);
    vtkSmartPyObject args(PyTuple_New(1));
    PyTuple_SET_ITEM(args.GetPointer(), 0, pydata.ReleaseReference());
    result.TakeReference(
      PyObject_Call(this->Internals->TransformMethod, args, nullptr));
    if (!result) {
      qCritical("Failed to execute the script.");
      checkForPythonError();
      return false;
    }
  }

  // Look for additional outputs from the filter returned in a dictionary
  PyObject* outputDict = result.GetPointer();

  int check = 0;
  {
    vtkPythonScopeGilEnsurer gilEnsurer(true);
    check = PyDict_Check(outputDict);
  }

  bool errorEncountered = false;
  if (check) {

    // Results (tables, etc.)
    for (int i = 0; i < m_resultNames.size(); ++i) {
      QByteArray byteArray = m_resultNames[i].toLatin1();
      const char* name = byteArray.data();
      PyObject* pyDataObject = nullptr;
      {
        vtkPythonScopeGilEnsurer gilEnsurer(true);
        pyDataObject = PyDict_GetItemString(outputDict, name);
      }
      if (!pyDataObject) {
        errorEncountered = true;
        qCritical() << "No result named" << m_resultNames[i]
                    << "defined in output dictionary.\n";
        continue;
      }
      vtkObjectBase* vtkobject =
        vtkPythonUtil::GetPointerFromObject(pyDataObject, "vtkDataObject");
      vtkDataObject* dataObject = vtkDataObject::SafeDownCast(vtkobject);
      if (dataObject) {
        // Emit signal so we switch back to UI thread
        emit newOperatorResult(m_resultNames[i], dataObject);
      } else {
        qCritical() << "Result named '" << name << "' is not a vtkDataObject";
        continue;
      }
    }

    // Segmentations, reconstructions, etc.
    for (int i = 0; i < m_childDataSourceNamesAndLabels.size(); ++i) {
      QPair<QString, QString> nameLabelPair =
        m_childDataSourceNamesAndLabels[i];
      QString name(nameLabelPair.first);
      QString label(nameLabelPair.second);
      PyObject* child = nullptr;
      {
        vtkPythonScopeGilEnsurer gilEnsurer(true);
        child = PyDict_GetItemString(outputDict, name.toLatin1().data());
      }
      if (!child) {
        errorEncountered = true;
        qCritical() << "No child data source named '" << name
                    << "' defined in output dictionary.\n";
        continue;
      }

      vtkObjectBase* vtkobject =
        vtkPythonUtil::GetPointerFromObject(child, "vtkDataObject");
      vtkSmartPointer<vtkDataObject> childData =
        vtkDataObject::SafeDownCast(vtkobject);
      if (childData) {
        emit newChildDataSource(label, childData);
      }
    }

    if (errorEncountered) {
      const char* objectReprString = nullptr;
      {
        vtkPythonScopeGilEnsurer gilEnsurer(true);
        PyObject* objectRepr = PyObject_Repr(outputDict);
        objectReprString = PyString_AsString(objectRepr);
      }
      qCritical() << "Dictionary return from Python script is:\n"
                  << objectReprString;
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

  return true;
}

bool OperatorPython::deserialize(const pugi::xml_node& ns)
{
  this->setJSONDescription(ns.attribute("json_description").as_string());
  this->setLabel(ns.attribute("label").as_string());
  this->setScript(ns.attribute("script").as_string());
  return true;
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
    vtkSMSourceProxy::SafeDownCast(producerProxy), DataSource::Volume, this);

  childDS->setFilename(label.toLatin1().data());
  this->setChildDataSource(childDS);
}

void OperatorPython::setOperatorResult(const QString& name,
                                       vtkSmartPointer<vtkDataObject> result)
{
  bool resultWasSet = this->setResult(name.toLatin1().data(), result);
  if (!resultWasSet) {
    qCritical() << "Could not set result '" << name << "'";
  }
}
}

#include "OperatorPython.moc"
