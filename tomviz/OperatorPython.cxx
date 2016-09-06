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

#include <QPointer>
#include <QtDebug>

#include "DataSource.h"
#include "EditOperatorWidget.h"
#include "OperatorResult.h"
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
#include <sstream>

#include "vtk_jsoncpp.h"

#include "ui_EditPythonOperatorWidget.h"

namespace {

bool CheckForError()
{
  vtkPythonScopeGilEnsurer gilEnsurer(true);
  PyObject* exception = PyErr_Occurred();
  if (exception) {
    PyErr_Print();
    PyErr_Clear();
    return true;
  }
  return false;
}

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

#include "OperatorPython.moc"
}

namespace tomviz {

class OperatorPython::OPInternals
{
public:
  vtkSmartPyObject OperatorModule;
  vtkSmartPyObject Code;
  vtkSmartPyObject TransformMethod;
};

OperatorPython::OperatorPython(QObject* parentObject)
  : Superclass(parentObject), Internals(new OperatorPython::OPInternals()),
    Label("Python Operator")
{
  vtkPythonInterpreter::Initialize();

  PyObject* pyObj = nullptr;
  {
    vtkPythonScopeGilEnsurer gilEnsurer(true);
    pyObj = PyImport_ImportModule("tomviz.utils");
  }

  this->Internals->OperatorModule.TakeReference(pyObj);
  if (!this->Internals->OperatorModule) {
    qCritical() << "Failed to import tomviz.utils module.";
    CheckForError();
  }

  // This connection is needed so we can create new child data sources in the UI
  // thread from a pipeline worker threads.
  connect(this, SIGNAL(newChildDataSource(const QString&, pqSMProxy)), this,
          SLOT(createNewChildDataSource(const QString&, pqSMProxy)));
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

  Json::Value root;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(str.toLatin1().data(), root);
  if (!parsingSuccessful) {
    qCritical() << "Failed to parse operator JSON";
    qCritical() << str;
    return;
  }

  // Get the label for the operator
  Json::Value labelNode = root["label"];
  if (!labelNode.isNull()) {
    setLabel(labelNode.asCString());
  }

  m_resultNames.clear();
  m_childDataSourceNamesAndLabels.clear();

  // Get the number of results
  Json::Value resultsNode = root["results"];
  if (!resultsNode.isNull()) {
    Json::Value::ArrayIndex numResults = resultsNode.size();
    setNumberOfResults(numResults);

    for (Json::Value::ArrayIndex i = 0; i < numResults; ++i) {
      OperatorResult* oa = resultAt(i);
      if (!oa) {
        Q_ASSERT(oa != nullptr);
      }
      Json::Value nameValue = resultsNode[i]["name"];
      if (!nameValue.isNull()) {
        oa->setName(nameValue.asCString());
        m_resultNames.append(nameValue.asCString());
      }
      Json::Value labelValue = resultsNode[i]["label"];
      if (!labelValue.isNull()) {
        oa->setLabel(labelValue.asCString());
      }
    }
  }

  // Get child dataset information
  Json::Value childDatasetNode = root["children"];
  if (!childDatasetNode.isNull()) {
    Json::Value::ArrayIndex size = childDatasetNode.size();
    if (size != 1) {
      qCritical() << "Only one child dataset is supported for now. Found"
                  << size << " but only the first will be used";
    }
    if (size > 0) {
      setHasChildDataSource(true);
      Json::Value nameValue = childDatasetNode[0]["name"];
      Json::Value labelValue = childDatasetNode[0]["label"];
      if (!nameValue.isNull() && !labelValue.isNull()) {
        QPair<QString, QString> nameLabelPair(QString(nameValue.asCString()),
                                              QString(labelValue.asCString()));
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
    this->Internals->TransformMethod.TakeReference(nullptr);

    PyObject* pyObj = nullptr;
    {
      vtkPythonScopeGilEnsurer gilEnsurer(true);
      pyObj = Py_CompileString(this->Script.toLatin1().data(),
                               this->label().toLatin1().data(),
                               Py_file_input /*Py_eval_input*/);
    }

    this->Internals->Code.TakeReference(pyObj);
    if (!this->Internals->Code) {
      CheckForError();
      qCritical(
        "Invalid script. Please check the traceback message for details");
      return;
    }

    vtkSmartPyObject module;
    {
      vtkPythonScopeGilEnsurer gilEnsurer(true);
      pyObj = PyImport_ExecCodeModule(
        QString("tomviz_%1").arg(this->label()).toLatin1().data(),
        this->Internals->Code);
    }

    module.TakeReference(pyObj);
    if (!module) {
      CheckForError();
      qCritical("Failed to create module.");
      return;
    }

    {
      vtkPythonScopeGilEnsurer gilEnsurer(true);
      pyObj = PyObject_GetAttrString(module, "transform_scalars");
    }

    this->Internals->TransformMethod.TakeReference(pyObj);
    if (!this->Internals->TransformMethod) {
      CheckForError();
      qWarning("Script doesn't have any 'transform_scalars' function.");
      return;
    }
    CheckForError();
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
  PyObject* pyObj = nullptr;
  {
    vtkPythonScopeGilEnsurer gilEnsurer(true);
    pyObj = PyTuple_New(1);
  }
  vtkSmartPyObject args(pyObj);

  {
    vtkPythonScopeGilEnsurer gilEnsurer(true);
    PyTuple_SET_ITEM(args.GetPointer(), 0, pydata.ReleaseReference());
    pyObj = PyObject_Call(this->Internals->TransformMethod, args, nullptr);
  }
  vtkSmartPyObject result(pyObj);
  if (!result) {
    qCritical("Failed to execute the script.");
    CheckForError();
    return false;
  }

  // Look for additional outputs from the filter returned in a dictionary
  PyObject* outputDict = result.GetPointer();

  int check = 0;
  {
    vtkPythonScopeGilEnsurer gilEnsurer(true);
    check = PyDict_Check(outputDict);
  }

  if (check) {
    bool errorEncountered = false;

    // Results (tables, etc.)
    for (int i = 0; i < m_resultNames.size(); ++i) {
      const char* name = m_resultNames[i].toLatin1().data();
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
        bool resultWasSet = setResult(name, dataObject);
        if (!resultWasSet) {
          qCritical() << "Could not set result '" << name << "'";
          continue;
        }
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
      vtkDataObject* childData = vtkDataObject::SafeDownCast(vtkobject);
      if (childData) {
        vtkSMProxyManager* proxyManager = vtkSMProxyManager::GetProxyManager();
        vtkSMSessionProxyManager* sessionProxyManager =
          proxyManager->GetActiveSessionProxyManager();

        pqSMProxy producerProxy;
        producerProxy.TakeReference(
          sessionProxyManager->NewProxy("sources", "TrivialProducer"));
        producerProxy->UpdateVTKObjects();

        vtkTrivialProducer* producer = vtkTrivialProducer::SafeDownCast(
          producerProxy->GetClientSideObject());
        if (!producer) {
          qWarning() << "Could not get TrivialProducer from proxy";
          return false;
        }

        producer->SetOutput(childData);

        emit newChildDataSource(label, producerProxy);
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

  return CheckForError() == false;
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
  ns.append_attribute("label").set_value(this->label().toLatin1().data());
  ns.append_attribute("script").set_value(this->script().toLatin1().data());
  return true;
}

bool OperatorPython::deserialize(const pugi::xml_node& ns)
{
  this->setLabel(ns.attribute("label").as_string());
  this->setScript(ns.attribute("script").as_string());
  return true;
}

EditOperatorWidget* OperatorPython::getEditorContents(QWidget* p)
{
  return new EditPythonOperatorWidget(p, this);
}

void OperatorPython::createNewChildDataSource(
  const QString& label, vtkSmartPointer<vtkSMProxy> producerProxy)
{

  DataSource* childDS = new DataSource(
    vtkSMSourceProxy::SafeDownCast(producerProxy), DataSource::Volume, this);

  childDS->setFilename(label.toLatin1().data());
  this->setChildDataSource(childDS);
}
}
