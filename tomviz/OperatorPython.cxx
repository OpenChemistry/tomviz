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
#include "vtkPython.h"
#include "OperatorPython.h"

#include <QtDebug>

#include "vtkDataObject.h"
#include "vtkPythonInterpreter.h"
#include "vtkPythonUtil.h"
#include <sstream>

namespace
{
  // Smart pointer for PyObjects. Calls Py_XDECREF when scope ends.
  class SmartPyObject
    {
    PyObject *Object;

  public:
    SmartPyObject(PyObject *obj = NULL)
      : Object(obj)
      {
      }
    ~SmartPyObject()
      {
      Py_XDECREF(this->Object);
      }
    PyObject *operator->() const
      {
      return this->Object;
      }
    PyObject *GetPointer() const
      {
      return this->Object;
      }
    operator bool () const
      {
      return this->Object != NULL;
      }
    operator PyObject* () const
      {
      return this->Object;
      }

    void TakeReference(PyObject* obj)
      {
      if (this->Object)
        {
        Py_DECREF(this->Object);
        }
      this->Object = obj;
      }
    PyObject* ReleaseReference()
      {
      PyObject* ret = this->Object;
      this->Object = NULL;
      return ret;
      }
  private:
    SmartPyObject(const SmartPyObject&);
    void operator=(const SmartPyObject&) const;
    };

  //----------------------------------------------------------------------------
  bool CheckForError()
    {
    PyObject *exception = PyErr_Occurred();
    if (exception)
      {
      PyErr_Print();
      PyErr_Clear();
      return true;
      }
    return false;
    }
}

namespace tomviz
{

class OperatorPython::OPInternals
{
public:
  SmartPyObject OperatorModule;
  SmartPyObject Code;
  SmartPyObject TransformMethod;
};

//-----------------------------------------------------------------------------
OperatorPython::OperatorPython(QObject* parentObject) :
  Superclass(parentObject),
  Internals(new OperatorPython::OPInternals()),
  Label("Python Operator")
{
  vtkPythonInterpreter::Initialize();
  this->Internals->OperatorModule.TakeReference(PyImport_ImportModule("tomviz.utils"));
  if (!this->Internals->OperatorModule)
    {
    qCritical() << "Failed to import tomviz.utils module.";
    CheckForError();
    }
}

//-----------------------------------------------------------------------------
OperatorPython::~OperatorPython()
{
}

//-----------------------------------------------------------------------------
void OperatorPython::setLabel(const QString& txt)
{
  this->Label = txt;
  emit labelModified();
}

//-----------------------------------------------------------------------------
QIcon OperatorPython::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqProgrammableFilter24.png");
}

//-----------------------------------------------------------------------------
void OperatorPython::setScript(const QString& str)
{
  if (this->Script != str)
    {
    this->Script = str;
    this->Internals->Code.TakeReference(NULL);
    this->Internals->TransformMethod.TakeReference(NULL);

    this->Internals->Code.TakeReference(Py_CompileString(
        this->Script.toLatin1().data(),
        this->label().toLatin1().data(),
        Py_file_input/*Py_eval_input*/));
    if (!this->Internals->Code)
      {
      CheckForError();
      qCritical("Invalid script. Please check the traceback message for details");
      return;
      }

    SmartPyObject module;
    module.TakeReference(PyImport_ExecCodeModule(
        QString("tomviz_%1").arg(this->label()).toLatin1().data(),
        this->Internals->Code));
    if (!module)
      {
      CheckForError();
      qCritical("Failed to create module.");
      return;
      }

    this->Internals->TransformMethod.TakeReference(
      PyObject_GetAttrString(module, "transform_scalars"));
    if (!this->Internals->TransformMethod)
      {
      CheckForError();
      qWarning("Script doesn't have any 'transform' function.");
      return;
      }
    CheckForError();
    emit this->transformModified();
    }
}

//-----------------------------------------------------------------------------
bool OperatorPython::transform(vtkDataObject* data)
{
  if (this->Script.isEmpty()) { return true; }
  if (!this->Internals->OperatorModule || !this->Internals->TransformMethod)
    {
    return true;
    }

  Q_ASSERT(data);

  SmartPyObject pydata(vtkPythonUtil::GetObjectFromPointer(data));
  SmartPyObject args(PyTuple_New(1));
  PyTuple_SET_ITEM(args.GetPointer(), 0, pydata.ReleaseReference());

  SmartPyObject result;
  result.TakeReference(PyObject_Call(this->Internals->TransformMethod, args,
                                     NULL));
  if (!result)
    {
    qCritical("Failed to execute the script.");
    CheckForError();
    return false;
    }

  return CheckForError() == false;
}

//-----------------------------------------------------------------------------
Operator* OperatorPython::clone() const
{
  OperatorPython* newClone = new OperatorPython();
  newClone->setLabel(this->label());
  newClone->setScript(this->script());
  return newClone;
}

//-----------------------------------------------------------------------------
bool OperatorPython::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("label").set_value(this->label().toLatin1().data());
  ns.append_attribute("script").set_value(this->script().toLatin1().data());
  return true;
}

//-----------------------------------------------------------------------------
bool OperatorPython::deserialize(const pugi::xml_node& ns)
{
  this->setLabel(ns.attribute("label").as_string());
  this->setScript(ns.attribute("script").as_string());
  return true;
}

}
