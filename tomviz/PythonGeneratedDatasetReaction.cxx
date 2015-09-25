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

//----------------------------------------------------------------------------
class PythonGeneratedDataSource : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;
public:
//----------------------------------------------------------------------------
  PythonGeneratedDataSource(const QString &l, QObject* p = NULL)
    : Superclass(p), label(l)
  {
  }

//----------------------------------------------------------------------------
  ~PythonGeneratedDataSource() {}

//----------------------------------------------------------------------------
  void setScript(const QString &script)
  {
    vtkPythonInterpreter::Initialize();
    this->OperatorModule.TakeReference(PyImport_ImportModule("tomviz.utils"));
    if (!this->OperatorModule)
    {
      qCritical() << "Failed to import tomviz.utils module.";
      CheckForError();
    }
  
    this->Code.TakeReference(Py_CompileString(
          script.toLatin1().data(),
          this->label.toLatin1().data(),
          Py_file_input/*Py_eval_input*/));
    if (!this->Code)
    {
      CheckForError();
      qCritical() << "Invalid script. Please check the traceback message for details.";
      return;
    }

    vtkSmartPyObject module;
    module.TakeReference(PyImport_ExecCodeModule(
            QString("tomviz_%1").arg(this->label).toLatin1().data(),
            this->Code));
    if (!module)
    {
      CheckForError();
      qCritical() << "Failed to create module.";
      return;
    }
    this->GenerateFunction.TakeReference(
      PyObject_GetAttrString(module, "generate_dataset"));
    if (!this->GenerateFunction)
    {
      CheckForError();
      qCritical() << "Script does not have a 'generate_dataset' function.";
      return;
    }
    this->MakeDatasetFunction.TakeReference(
      PyObject_GetAttrString(this->OperatorModule, "make_dataset"));
    if (!this->MakeDatasetFunction)
    {
      CheckForError();
      qCritical() << "Could not find make_dataset function in tomviz.utils";
      return;
    }
    CheckForError();
  }

//----------------------------------------------------------------------------
  tomviz::DataSource *createDataSource(int shape[3])
  {
  vtkSmartPyObject args(PyTuple_New(4));
  PyTuple_SET_ITEM(args.GetPointer(), 0, PyInt_FromLong(shape[0]));
  PyTuple_SET_ITEM(args.GetPointer(), 1, PyInt_FromLong(shape[1]));
  PyTuple_SET_ITEM(args.GetPointer(), 2, PyInt_FromLong(shape[2]));
  PyTuple_SET_ITEM(args.GetPointer(), 3, this->GenerateFunction);

  vtkSmartPyObject result;
  result.TakeReference(PyObject_Call(this->MakeDatasetFunction, args, NULL));

  if (!result)
  {
    qCritical() << "Failed to execute script.";
    CheckForError();
    return NULL;
  }

  vtkImageData* image = vtkImageData::SafeDownCast(
      vtkPythonUtil::GetPointerFromObject(result,"vtkImageData"));
  if (image == NULL)
  {
    qCritical() << "Failed to get a valid image data from generation method.";
    CheckForError();
    return NULL;
  }

  vtkSMSessionProxyManager* pxm =
      tomviz::ActiveObjects::instance().proxyManager();
  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    source->GetClientSideObject());
  tp->SetOutput(image);
  tomviz::annotateDataProducer(source, this->label.toUtf8().data());

  CheckForError();

  return new tomviz::DataSource(vtkSMSourceProxy::SafeDownCast(source.Get()));
  }

private:
  vtkSmartPyObject OperatorModule;
  vtkSmartPyObject Code;
  vtkSmartPyObject GenerateFunction;
  vtkSmartPyObject MakeDatasetFunction;
  QString label;
};

//----------------------------------------------------------------------------
class ShapeWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;
public:
//----------------------------------------------------------------------------
  ShapeWidget(QWidget *p = NULL)
    : Superclass(p),
      xSpinBox(new QSpinBox(this)),
      ySpinBox(new QSpinBox(this)),
      zSpinBox(new QSpinBox(this))
  {
    QHBoxLayout* boundsLayout = new QHBoxLayout;

    QLabel* xLabel = new QLabel("X:", this);
    QLabel* yLabel = new QLabel("Y:", this);
    QLabel* zLabel = new QLabel("Z:", this);

    this->xSpinBox->setMaximum(std::numeric_limits<int>::max());
    this->xSpinBox->setMinimum(1);
    this->xSpinBox->setValue(100);
    this->ySpinBox->setMaximum(std::numeric_limits<int>::max());
    this->ySpinBox->setMinimum(1);
    this->ySpinBox->setValue(100);
    this->zSpinBox->setMaximum(std::numeric_limits<int>::max());
    this->zSpinBox->setMinimum(1);
    this->zSpinBox->setValue(100);

    boundsLayout->addWidget(xLabel);
    boundsLayout->addWidget(this->xSpinBox);
    boundsLayout->addWidget(yLabel);
    boundsLayout->addWidget(this->ySpinBox);
    boundsLayout->addWidget(zLabel);
    boundsLayout->addWidget(this->zSpinBox);
    
    this->setLayout(boundsLayout);
  }

//----------------------------------------------------------------------------
  ~ShapeWidget() {}

//----------------------------------------------------------------------------
  void getShape(int shape[3])
  {
    shape[0] = xSpinBox->value();
    shape[1] = ySpinBox->value();
    shape[2] = zSpinBox->value();
  }
private:
  Q_DISABLE_COPY(ShapeWidget)

  QSpinBox *xSpinBox;
  QSpinBox *ySpinBox;
  QSpinBox *zSpinBox;
};

}

#include "PythonGeneratedDatasetReaction.moc"

namespace tomviz
{

class PythonGeneratedDatasetReaction::PGDRInternal
{
public:
  QString scriptLabel;
  QString scriptSource;
};
//-----------------------------------------------------------------------------
PythonGeneratedDatasetReaction::PythonGeneratedDatasetReaction(QAction* parentObject,
                                                               const QString &l,
                                                               const QString &s)
  : Superclass(parentObject), Internals(new PGDRInternal)
{
  this->Internals->scriptLabel = l;
  this->Internals->scriptSource = s;
}

//-----------------------------------------------------------------------------
PythonGeneratedDatasetReaction::~PythonGeneratedDatasetReaction()
{
}

//-----------------------------------------------------------------------------
void PythonGeneratedDatasetReaction::addDataset()
{
  PythonGeneratedDataSource generator(this->Internals->scriptLabel);
  if (this->Internals->scriptLabel == "Zero Dataset")
  {
    QDialog dialog;
    dialog.setWindowTitle("Set Size");
    ShapeWidget *shapeWidget = new ShapeWidget(&dialog);

    // For other python scripts with parameters, create the GUI here

    QVBoxLayout* layout = new QVBoxLayout;
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Cancel|QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
    QObject::connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    // For other python scripts with parameters, add the GUI here
    layout->addWidget(shapeWidget);
    layout->addWidget(buttons);

    dialog.setLayout(layout);
    if (dialog.exec() != QDialog::Accepted)
    {
      return;
    }
    // For other python scripts with paramters, modify the script here
    generator.setScript(this->Internals->scriptSource);
    int shape[3];
    shapeWidget->getShape(shape);
    this->dataSourceAdded(generator.createDataSource(shape));
  }
  else if (this->Internals->scriptLabel == "Constant Dataset")
  {
    QDialog dialog;
    dialog.setWindowTitle("Set Parameters");
    ShapeWidget *shapeWidget = new ShapeWidget(&dialog);

    QLabel *label = new QLabel("Value: ", &dialog);
    QDoubleSpinBox *constant = new QDoubleSpinBox(&dialog);

    QHBoxLayout *parametersLayout = new QHBoxLayout;
    parametersLayout->addWidget(label);
    parametersLayout->addWidget(constant);

    QVBoxLayout* layout = new QVBoxLayout;
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Cancel|QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
    QObject::connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    layout->addWidget(shapeWidget);
    layout->addItem(parametersLayout);
    layout->addWidget(buttons);

    dialog.setLayout(layout);
    if (dialog.exec() != QDialog::Accepted)
    {
      return;
    }
    // substitute values
    QString localScript = this->Internals->scriptSource.replace(
        "###CONSTANT###", QString("CONSTANT = %1").arg(constant->value()));

    generator.setScript(localScript);
    int shape[3];
    shapeWidget->getShape(shape);
    this->dataSourceAdded(generator.createDataSource(shape));
  }
}

//-----------------------------------------------------------------------------
void PythonGeneratedDatasetReaction::dataSourceAdded(DataSource* dataSource)
{
  if (!dataSource)
  {
    return;
  }
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
