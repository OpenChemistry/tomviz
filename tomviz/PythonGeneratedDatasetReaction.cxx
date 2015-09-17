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

#include "vtkPython.h" // must be first
#include "PythonGeneratedDatasetReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"
#include "Utilities.h"
#include "vtkNew.h"
#include "vtkImageData.h"
#include "vtkPythonInterpreter.h"
#include "vtkPythonUtil.h"
#include "vtkSmartPointer.h"
#include "vtkSmartPyObject.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

namespace
{
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

class PythonGeneratedDatasetReaction::PGDRInternal
{
public:
  QString scriptLabel;
  QString scriptSource;
  vtkSmartPyObject OperatorModule;
  vtkSmartPyObject Code;
  vtkSmartPyObject GenerateFunction;
  vtkSmartPyObject MakeDatasetFunction;
};
//-----------------------------------------------------------------------------
PythonGeneratedDatasetReaction::PythonGeneratedDatasetReaction(QAction* parentObject,
                                                               const QString &l,
                                                               const QString &s)
  : Superclass(parentObject), Internals(new PGDRInternal)
{
  this->Internals->scriptLabel = l;
  vtkPythonInterpreter::Initialize();
  this->Internals->OperatorModule.TakeReference(PyImport_ImportModule("tomviz.utils"));
  if (!this->Internals->OperatorModule)
  {
    qCritical() << "Failed to import tomviz.utils module.";
    CheckForError();
  }
  this->Internals->scriptSource = s;

  this->Internals->Code.TakeReference(Py_CompileString(
        this->Internals->scriptSource.toLatin1().data(),
        this->Internals->scriptLabel.toLatin1().data(),
        Py_file_input/*Py_eval_input*/));
  if (!this->Internals->Code)
  {
    CheckForError();
    qCritical() << "Invalid script. Please check the traceback message for details.";
    return;
  }

  vtkSmartPyObject module;
  module.TakeReference(PyImport_ExecCodeModule(
          QString("tomviz_%1").arg(this->Internals->scriptLabel).toLatin1().data(),
          this->Internals->Code));
  if (!module)
  {
    CheckForError();
    qCritical() << "Failed to create module.";
    return;
  }
  this->Internals->GenerateFunction.TakeReference(
    PyObject_GetAttrString(module, "generate_dataset"));
  if (!this->Internals->GenerateFunction)
  {
    CheckForError();
    qCritical() << "Script does not have a 'generate_dataset' function.";
    return;
  }
  this->Internals->MakeDatasetFunction.TakeReference(
    PyObject_GetAttrString(this->Internals->OperatorModule, "make_dataset"));
  if (!this->Internals->MakeDatasetFunction)
  {
    CheckForError();
    qCritical() << "Could not find make_dataset function in tomviz.utils";
    return;
  }
  CheckForError();
}

//-----------------------------------------------------------------------------
PythonGeneratedDatasetReaction::~PythonGeneratedDatasetReaction()
{
}

//-----------------------------------------------------------------------------
void PythonGeneratedDatasetReaction::addDataset()
{
  QDialog dialog;
  dialog.setWindowTitle("Set Size");
  QHBoxLayout* boundsLayout = new QHBoxLayout;

  QLabel* xLabel = new QLabel("X:", &dialog);
  QLabel* yLabel = new QLabel("Y:", &dialog);
  QLabel* zLabel = new QLabel("Z:", &dialog);

  QSpinBox* xLength = new QSpinBox(&dialog);
  xLength->setMaximum(std::numeric_limits<int>::max());
  xLength->setMinimum(1);
  xLength->setValue(100);
  QSpinBox* yLength = new QSpinBox(&dialog);
  yLength->setMaximum(std::numeric_limits<int>::max());
  yLength->setMinimum(1);
  yLength->setValue(100);
  QSpinBox* zLength = new QSpinBox(&dialog);
  zLength->setMaximum(std::numeric_limits<int>::max());
  zLength->setMinimum(1);
  zLength->setValue(100);

  boundsLayout->addWidget(xLabel);
  boundsLayout->addWidget(xLength);
  boundsLayout->addWidget(yLabel);
  boundsLayout->addWidget(yLength);
  boundsLayout->addWidget(zLabel);
  boundsLayout->addWidget(zLength);

  QVBoxLayout* layout = new QVBoxLayout;
  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Cancel|QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
  QObject::connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

  layout->addItem(boundsLayout);
  layout->addWidget(buttons);

  dialog.setLayout(layout);
  if (dialog.exec() != QDialog::Accepted)
  {
    return;
  }

  vtkSmartPyObject args(PyTuple_New(4));
  PyTuple_SET_ITEM(args.GetPointer(), 0, PyInt_FromLong(xLength->value()));
  PyTuple_SET_ITEM(args.GetPointer(), 1, PyInt_FromLong(yLength->value()));
  PyTuple_SET_ITEM(args.GetPointer(), 2, PyInt_FromLong(zLength->value()));
  PyTuple_SET_ITEM(args.GetPointer(), 3, this->Internals->GenerateFunction);

  vtkSmartPyObject result;
  result.TakeReference(PyObject_Call(this->Internals->MakeDatasetFunction, args, NULL));

  if (!result)
  {
    qCritical() << "Failed to execute script.";
    CheckForError();
    return;
  }

  vtkImageData* image = vtkImageData::SafeDownCast(
      vtkPythonUtil::GetPointerFromObject(result,"vtkImageData"));
  if (image == NULL)
  {
    qCritical() << "Failed to get a valid image data from generation method.";
    CheckForError();
    return;
  }

  vtkSMSessionProxyManager* pxm =
      ActiveObjects::instance().proxyManager();
  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    source->GetClientSideObject());
  tp->SetOutput(image);
  tomviz::annotateDataProducer(source, this->Internals->scriptLabel.toUtf8().data());

  DataSource *dSource = new DataSource(vtkSMSourceProxy::SafeDownCast(source.Get()));

  dataSourceAdded(dSource);

  CheckForError();
}

//-----------------------------------------------------------------------------
void PythonGeneratedDatasetReaction::dataSourceAdded(DataSource* dataSource)
{
  ModuleManager::instance().addDataSource(dataSource);

  vtkSMViewProxy* view = ActiveObjects::instance().activeView();
  // Create an outline module for the source in the active view.
  if (Module* module = ModuleManager::instance().createAndAddModule(
      "Outline", dataSource, view))
    {
    ActiveObjects::instance().setActiveModule(module);
    }
  if (Module* module = ModuleManager::instance().createAndAddModule(
        "Orthogonal Slice", dataSource, view))
    {
    ActiveObjects::instance().setActiveModule(module);
    }
}

}
