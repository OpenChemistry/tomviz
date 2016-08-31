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
  this->Internals->OperatorModule.TakeReference(
    PyImport_ImportModule("tomviz.utils"));
  if (!this->Internals->OperatorModule) {
    qCritical() << "Failed to import tomviz.utils module.";
    CheckForError();
  }
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
  bool parsingSuccessful = reader.parse(str.toStdString().c_str(), root);
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
      }
      Json::Value labelValue = resultsNode[i]["label"];
      if (!labelValue.isNull()) {
        oa->setLabel(labelValue.asCString());
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

    this->Internals->Code.TakeReference(Py_CompileString(
      this->Script.toLatin1().data(), this->label().toLatin1().data(),
      Py_file_input /*Py_eval_input*/));
    if (!this->Internals->Code) {
      CheckForError();
      qCritical(
        "Invalid script. Please check the traceback message for details");
      return;
    }

    vtkSmartPyObject module;
    module.TakeReference(PyImport_ExecCodeModule(
      QString("tomviz_%1").arg(this->label()).toLatin1().data(),
      this->Internals->Code));
    if (!module) {
      CheckForError();
      qCritical("Failed to create module.");
      return;
    }

    this->Internals->TransformMethod.TakeReference(
      PyObject_GetAttrString(module, "transform_scalars"));
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
  vtkSmartPyObject args(PyTuple_New(1));
  PyTuple_SET_ITEM(args.GetPointer(), 0, pydata.ReleaseReference());

  vtkSmartPyObject result;
  result.TakeReference(
    PyObject_Call(this->Internals->TransformMethod, args, nullptr));
  if (!result) {
    qCritical("Failed to execute the script.");
    CheckForError();
    return false;
  }

  // TODO - check if there are results but no settings for them in
  // the output from the script.

  // Look for additional outputs from the filter returned in a dictionary
  PyObject* outputDict = result.GetPointer();
  if (PyDict_Check(outputDict)) {

    // Results (tables, etc.)
    for (int i = 0; i < numberOfResults(); ++i) {
      OperatorResult* operatorResult = resultAt(i);
      std::string resultName = operatorResult->name().toStdString();
      PyObject* pyDataObject =
        PyDict_GetItemString(outputDict, resultName.c_str());
      if (pyDataObject) {
        vtkObjectBase* vtkobject =
          vtkPythonUtil::GetPointerFromObject(pyDataObject, "vtkDataObject");
        if (vtkobject) {
          setResult(i, vtkDataObject::SafeDownCast(vtkobject));
        }
      } else {
        qCritical()
          << "No result named" << ("'" + resultName + "'").c_str()
          << "defined in output dictionary from 'transform_scalars' function.";
      }
    }

    // Segmentations (label maps, etc.)
    PyObject* children = PyDict_GetItemString(outputDict, "children");
    if (children && PyDict_Check(children)) {
      PyObject *key, *value;
      Py_ssize_t pos = 0;
      while (PyDict_Next(children, &pos, &key, &value)) {
        const char* name = PyString_AsString(key);
        if (!PyDict_Check(value)) {
          qWarning() << "value is not a dict. It is a"
                     << value->ob_type->tp_name << "instead" << endl;
          continue;
        }
        PyObject* dataSetObject = PyDict_GetItemString(value, "data_set");
        if (!dataSetObject) {
          qWarning() << "no 'data_set' defined in child object";
          continue;
        }
        vtkObjectBase* vtkobject =
          vtkPythonUtil::GetPointerFromObject(dataSetObject, "vtkDataObject");
        vtkDataObject* childData = vtkDataObject::SafeDownCast(vtkobject);
        if (childData) {
          cout << "data_set is " << *childData << endl;

          vtkSMProxyManager* proxyManager =
            vtkSMProxyManager::GetProxyManager();
          vtkSMSessionProxyManager* sessionProxyManager =
            proxyManager->GetActiveSessionProxyManager();

          vtkSmartPointer<vtkSMProxy> producerProxy;
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

          DataSource* childDS =
            new DataSource(vtkSMSourceProxy::SafeDownCast(producerProxy),
                           DataSource::Volume, this);
          setChildDataSource(childDS);
        }
      }
    } else if (children != nullptr) {
      qWarning() << "'children' entry was not a dictionary, but must be";
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
}
