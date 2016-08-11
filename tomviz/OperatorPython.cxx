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

#include <QPointer>
#include <QtDebug>

#include "EditOperatorWidget.h"
#include "OperatorResult.h"
#include "pqPythonSyntaxHighlighter.h"
#include "vtkDataObject.h"
#include "vtkPythonInterpreter.h"
#include "vtkPythonUtil.h"
#include "vtkSmartPyObject.h"
#include <sstream>

#include "ui_EditPythonOperatorWidget.h"

namespace
{

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

  class EditPythonOperatorWidget : public tomviz::EditOperatorWidget
  {
    Q_OBJECT
    typedef tomviz::EditOperatorWidget Superclass;
  public:
    EditPythonOperatorWidget(QWidget *p, tomviz::OperatorPython *o)
      : Superclass(p), Op(o), Ui()
    {
      this->Ui.setupUi(this);
      this->Ui.name->setText(o->label());
      if (!o->script().isEmpty())
      {
        this->Ui.script->setPlainText(o->script());
      }
      new pqPythonSyntaxHighlighter(this->Ui.script, this);
    }
    void applyChangesToOperator() override
    {
      if (this->Op)
      {
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

namespace tomviz
{

class OperatorPython::OPInternals
{
public:
  vtkSmartPyObject OperatorModule;
  vtkSmartPyObject Code;
  vtkSmartPyObject TransformMethod;
};

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

void OperatorPython::setScript(const QString& str)
{
  if (this->Script != str)
  {
    this->Script = str;
    this->Internals->Code.TakeReference(nullptr);
    this->Internals->TransformMethod.TakeReference(nullptr);

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

    vtkSmartPyObject module;
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
      qWarning("Script doesn't have any 'transform_scalars' function.");
      return;
    }
    CheckForError();
    emit this->transformModified();
  }
}

bool OperatorPython::applyTransform(vtkDataObject* data)
{
  if (this->Script.isEmpty()) { return true; }
  if (!this->Internals->OperatorModule || !this->Internals->TransformMethod)
  {
    return true;
  }

  Q_ASSERT(data);

  vtkSmartPyObject pydata(vtkPythonUtil::GetObjectFromPointer(data));
  vtkSmartPyObject args(PyTuple_New(1));
  PyTuple_SET_ITEM(args.GetPointer(), 0, pydata.ReleaseReference());

  vtkSmartPyObject result;
  result.TakeReference(PyObject_Call(this->Internals->TransformMethod, args,
                                     nullptr));
  if (!result)
  {
    qCritical("Failed to execute the script.");
    CheckForError();
    return false;
  }

  return CheckForError() == false;
}

Operator* OperatorPython::clone() const
{
  OperatorPython* newClone = new OperatorPython();
  newClone->setLabel(this->label());
  newClone->setScript(this->script());
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

EditOperatorWidget *OperatorPython::getEditorContents(QWidget *p)
{
  return new EditPythonOperatorWidget(p, this);
}

}
