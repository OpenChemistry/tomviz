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

#include "PythonGeneratedDatasetReaction.h"
#include "vtkPython.h" // must be first

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"
#include "Utilities.h"
#include "pqActiveObjects.h"
#include "pqRenderView.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkPythonInterpreter.h"
#include "vtkPythonUtil.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSmartPointer.h"
#include "vtkSmartPyObject.h"
#include "vtkTrivialProducer.h"

#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

namespace {

class PythonGeneratedDataSource : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  PythonGeneratedDataSource(const QString& l, QObject* p = nullptr)
    : Superclass(p), label(l)
  {
  }

  ~PythonGeneratedDataSource() {}

  void setScript(const QString& script)
  {
    vtkPythonInterpreter::Initialize();
    this->OperatorModule.TakeReference(PyImport_ImportModule("tomviz.utils"));
    if (!this->OperatorModule) {
      qCritical() << "Failed to import tomviz.utils module.";
      tomviz::checkForPythonError();
    }

    this->Code.TakeReference(Py_CompileString(script.toLatin1().data(),
                                              this->label.toLatin1().data(),
                                              Py_file_input /*Py_eval_input*/));
    if (!this->Code) {
      tomviz::checkForPythonError();
      qCritical()
        << "Invalid script. Please check the traceback message for details.";
      return;
    }

    vtkSmartPyObject module;
    module.TakeReference(PyImport_ExecCodeModule(
      // Don't let these be the same, even for similar scripts.  Seems to cause
      // python crashes.
      QString("tomviz_%1%2")
        .arg(this->label)
        .arg(number_of_scripts++)
        .toLatin1()
        .data(),
      this->Code));
    if (!module) {
      tomviz::checkForPythonError();
      qCritical() << "Failed to create module.";
      return;
    }
    this->GenerateFunction.TakeReference(
      PyObject_GetAttrString(module, "generate_dataset"));
    if (!this->GenerateFunction) {
      tomviz::checkForPythonError();
      qCritical() << "Script does not have a 'generate_dataset' function.";
      return;
    }
    this->MakeDatasetFunction.TakeReference(
      PyObject_GetAttrString(this->OperatorModule, "make_dataset"));
    if (!this->MakeDatasetFunction) {
      tomviz::checkForPythonError();
      qCritical() << "Could not find make_dataset function in tomviz.utils";
      return;
    }
    tomviz::checkForPythonError();
    this->pythonScript = script;
  }

  vtkSmartPointer<vtkSMSourceProxy> createDataSource(const int shape[3])
  {
    vtkNew<vtkImageData> image;
    vtkSmartPointer<vtkSMSourceProxy> retVal;
    vtkSmartPyObject args(PyTuple_New(5));
    PyTuple_SET_ITEM(args.GetPointer(), 0, PyInt_FromLong(shape[0]));
    PyTuple_SET_ITEM(args.GetPointer(), 1, PyInt_FromLong(shape[1]));
    PyTuple_SET_ITEM(args.GetPointer(), 2, PyInt_FromLong(shape[2]));
    PyTuple_SET_ITEM(args.GetPointer(), 3,
                     vtkPythonUtil::GetObjectFromPointer(image.Get()));
    PyTuple_SET_ITEM(args.GetPointer(), 4, this->GenerateFunction);

    vtkSmartPyObject result;
    result.TakeReference(
      PyObject_Call(this->MakeDatasetFunction, args, nullptr));

    if (!result) {
      qCritical() << "Failed to execute script.";
      tomviz::checkForPythonError();
      return retVal;
    }

    vtkSMSessionProxyManager* pxm =
      tomviz::ActiveObjects::instance().proxyManager();
    vtkSmartPointer<vtkSMProxy> source;
    source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
    vtkTrivialProducer* tp =
      vtkTrivialProducer::SafeDownCast(source->GetClientSideObject());
    tp->SetOutput(image.Get());
    source->SetAnnotation("tomviz.Type", "DataSource");
    source->SetAnnotation("tomviz.DataSource.FileName",
                          "Python Generated Data");
    source->SetAnnotation("tomviz.Label", this->label.toLatin1().data());
    source->SetAnnotation("tomviz.Python_Source.Script",
                          this->pythonScript.toLatin1().data());
    source->SetAnnotation("tomviz.Python_Source.X",
                          QString::number(shape[0]).toLatin1().data());
    source->SetAnnotation("tomviz.Python_Source.Y",
                          QString::number(shape[1]).toLatin1().data());
    source->SetAnnotation("tomviz.Python_Source.Z",
                          QString::number(shape[2]).toLatin1().data());

    tomviz::checkForPythonError();

    retVal = vtkSMSourceProxy::SafeDownCast(source.Get());
    return retVal;
  }

private:
  vtkSmartPyObject OperatorModule;
  vtkSmartPyObject Code;
  vtkSmartPyObject GenerateFunction;
  vtkSmartPyObject MakeDatasetFunction;
  QString label;
  QString pythonScript;
  static int number_of_scripts;
};

int PythonGeneratedDataSource::number_of_scripts = 0;

class ShapeWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  ShapeWidget(QWidget* p = nullptr)
    : Superclass(p), xSpinBox(new QSpinBox(this)), ySpinBox(new QSpinBox(this)),
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

  ~ShapeWidget() {}

  void getShape(int shape[3])
  {
    shape[0] = xSpinBox->value();
    shape[1] = ySpinBox->value();
    shape[2] = zSpinBox->value();
  }

  void setSpinBoxValue(int newX, int newY, int newZ)
  {
    this->xSpinBox->setValue(newX);
    this->ySpinBox->setValue(newY);
    this->zSpinBox->setValue(newZ);
  }

  void setSpinBoxMaximum(int xmax, int ymax, int zmax)
  {
    this->xSpinBox->setMaximum(xmax);
    this->ySpinBox->setMaximum(ymax);
    this->zSpinBox->setMaximum(zmax);
  }

private:
  Q_DISABLE_COPY(ShapeWidget)

  QSpinBox* xSpinBox;
  QSpinBox* ySpinBox;
  QSpinBox* zSpinBox;
};
}

#include "PythonGeneratedDatasetReaction.moc"

namespace tomviz {

class PythonGeneratedDatasetReaction::PGDRInternal
{
public:
  QString scriptLabel;
  QString scriptSource;
};

PythonGeneratedDatasetReaction::PythonGeneratedDatasetReaction(
  QAction* parentObject, const QString& l, const QString& s)
  : Superclass(parentObject), Internals(new PGDRInternal)
{
  this->Internals->scriptLabel = l;
  this->Internals->scriptSource = s;
}

PythonGeneratedDatasetReaction::~PythonGeneratedDatasetReaction()
{
}

void PythonGeneratedDatasetReaction::addDataset()
{
  PythonGeneratedDataSource generator(this->Internals->scriptLabel);
  if (this->Internals->scriptLabel == "Constant Dataset") {
    QDialog dialog;
    dialog.setWindowTitle("Generate Constant Dataset");
    ShapeWidget* shapeWidget = new ShapeWidget(&dialog);

    QLabel* label = new QLabel("Value: ", &dialog);
    QDoubleSpinBox* constant = new QDoubleSpinBox(&dialog);

    QHBoxLayout* parametersLayout = new QHBoxLayout;
    parametersLayout->addWidget(label);
    parametersLayout->addWidget(constant);

    QVBoxLayout* layout = new QVBoxLayout;
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
    QObject::connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    layout->addWidget(shapeWidget);
    layout->addItem(parametersLayout);
    layout->addWidget(buttons);

    dialog.setLayout(layout);
    if (dialog.exec() != QDialog::Accepted) {
      return;
    }
    // substitute values
    QString localScript = this->Internals->scriptSource.replace(
      "###CONSTANT###", QString("CONSTANT = %1").arg(constant->value()));

    generator.setScript(localScript);
    int shape[3];
    shapeWidget->getShape(shape);
    this->dataSourceAdded(generator.createDataSource(shape));
  } else if (this->Internals->scriptLabel == "Random Particles") {
    QDialog dialog;
    dialog.setWindowTitle("Generate Random Particles");
    QVBoxLayout* layout = new QVBoxLayout; // overall layout
    // Guide
    QLabel* guide = new QLabel;
    guide->setText("Generate many random 3D \"particles\" using the Fourier "
                   "Noise method. You can increase the \"Internal Complexity\" "
                   "of particles and their average \"Particle Size\". You can "
                   "also specify the sparsity (percentage of non-zero voxels) "
                   "of the generated dataset. Note: 512x512x512 may take a "
                   "couple minutes to run.");
    guide->setWordWrap(true);
    layout->addWidget(guide);

    // Shape Layout
    ShapeWidget* shapeLayout = new ShapeWidget(&dialog);
    shapeLayout->setSpinBoxValue(128, 128, 128);
    shapeLayout->setSpinBoxMaximum(512, 512, 512);
    // Parameter Layout
    QGridLayout* parametersLayout = new QGridLayout;

    QLabel* label = new QLabel("Internal Complexity ([1-100]): ", &dialog);
    parametersLayout->addWidget(label, 0, 0, 1, 2);

    QDoubleSpinBox* innerStructureParameter = new QDoubleSpinBox(&dialog);
    innerStructureParameter->setRange(1, 100);
    innerStructureParameter->setValue(30);
    innerStructureParameter->setSingleStep(5);
    parametersLayout->addWidget(innerStructureParameter, 0, 2, 1, 1);

    label = new QLabel("Particle Size ([1-100]): ", &dialog);
    parametersLayout->addWidget(label, 1, 0, 1, 2);

    QDoubleSpinBox* shapeParameter = new QDoubleSpinBox(&dialog);
    shapeParameter->setRange(1, 100);
    shapeParameter->setValue(60);
    shapeParameter->setSingleStep(5);
    parametersLayout->addWidget(shapeParameter, 1, 2, 1, 1);

    label = new QLabel("Sparsity (percentage of non-zero voxels): ", &dialog);
    parametersLayout->addWidget(label, 2, 0, 2, 1);

    QDoubleSpinBox* sparsityParameter = new QDoubleSpinBox(&dialog);
    sparsityParameter->setRange(0, 1);
    sparsityParameter->setValue(0.2);
    sparsityParameter->setSingleStep(0.05);
    parametersLayout->addWidget(sparsityParameter, 2, 2, 1, 1);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
    QObject::connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    layout->addWidget(shapeLayout);
    layout->addItem(parametersLayout);
    layout->addWidget(buttons);
    dialog.setLayout(layout);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable

    // substitute values
    if (dialog.exec() == QDialog::Accepted) {
      QString pythonScript = this->Internals->scriptSource;
      pythonScript.replace(
        "###p_in###",
        QString("p_in = %1").arg(innerStructureParameter->value()));
      pythonScript.replace("###p_s###",
                           QString("p_s = %1").arg(shapeParameter->value()));
      pythonScript.replace(
        "###sparsity###",
        QString("sparsity = %1").arg(sparsityParameter->value()));

      generator.setScript(pythonScript);
      int shape[3];
      shapeLayout->getShape(shape);
      this->dataSourceAdded(generator.createDataSource(shape));
    }
  } else if (this->Internals->scriptLabel == "Electron Beam Shape") {
    QDialog dialog;
    dialog.setWindowTitle("Generate Electron Beam Shape");
    QVBoxLayout* layout = new QVBoxLayout; // overall layout
    // Guide
    QLabel* guide = new QLabel;
    guide->setText("Generate a convergent electron beam in 3D. This represents "
                   "the 3D probe used for atomic resolution imaging in a "
                   "scanning transmission electron microscope.");
    guide->setWordWrap(true);
    layout->addWidget(guide);

    // Parameter Layout
    QGridLayout* parametersLayout = new QGridLayout;

    QLabel* label = new QLabel("Beam energy (keV): ", &dialog);
    parametersLayout->addWidget(label, 0, 0, 1, 2);
    QDoubleSpinBox* voltage = new QDoubleSpinBox(&dialog);
    voltage->setRange(1, 1000000);
    voltage->setValue(300);
    voltage->setSingleStep(50);
    parametersLayout->addWidget(voltage, 0, 2, 1, 1);

    label = new QLabel("Semi-convergence angle (mrad): ", &dialog);
    parametersLayout->addWidget(label, 1, 0, 1, 2);
    QDoubleSpinBox* alpha_max = new QDoubleSpinBox(&dialog);
    alpha_max->setRange(0, 500);
    alpha_max->setValue(30);
    alpha_max->setSingleStep(0.5);
    parametersLayout->addWidget(alpha_max, 1, 2, 1, 1);

    label = new QLabel("Transerve plane (x-y): ", &dialog);
    parametersLayout->addWidget(label, 2, 0, 1, 1);
    label = new QLabel("Number of pixels: ", &dialog);
    parametersLayout->addWidget(label, 2, 1, 1, 1);
    QSpinBox* Nxy = new QSpinBox(&dialog);
    Nxy->setRange(64, 2048);
    Nxy->setValue(256);
    Nxy->setSingleStep(1);
    parametersLayout->addWidget(Nxy, 2, 2, 1, 1);

    label = new QLabel("x-y pixel size (angstrom): ", &dialog);
    parametersLayout->addWidget(label, 3, 1, 1, 1);
    QDoubleSpinBox* dxy = new QDoubleSpinBox(&dialog);
    dxy->setDecimals(4);
    dxy->setMinimum(0.0001);
    dxy->setValue(0.1);
    dxy->setSingleStep(0.1);
    parametersLayout->addWidget(dxy, 3, 2, 1, 1);

    label = new QLabel("Propagation direction (z): ", &dialog);
    parametersLayout->addWidget(label, 4, 0, 1, 1);
    label = new QLabel("Number of pixels: ", &dialog);
    parametersLayout->addWidget(label, 4, 1, 1, 1);
    QSpinBox* Nz = new QSpinBox(&dialog);
    Nz->setRange(1, 2048);
    Nz->setValue(512);
    Nz->setSingleStep(1);
    parametersLayout->addWidget(Nz, 4, 2, 1, 1);

    label = new QLabel("Minimum defocus (nm): ", &dialog);
    parametersLayout->addWidget(label, 5, 1, 1, 1);
    QDoubleSpinBox* df_min = new QDoubleSpinBox(&dialog);
    df_min->setRange(-1000000, 1000000);
    df_min->setValue(-50.0);
    df_min->setSingleStep(5.0);
    parametersLayout->addWidget(df_min, 5, 2, 1, 1);

    label = new QLabel("Maximum defocus (nm): ", &dialog);
    parametersLayout->addWidget(label, 6, 1, 1, 1);
    QDoubleSpinBox* df_max = new QDoubleSpinBox(&dialog);
    df_max->setRange(-1000000, 1000000);
    df_max->setValue(100.0);
    df_max->setSingleStep(5.0);
    parametersLayout->addWidget(df_max, 6, 2, 1, 1);

    label = new QLabel("Third-order spherical aberration (mm): ", &dialog);
    parametersLayout->addWidget(label, 0, 3, 1, 2);
    QDoubleSpinBox* c3 = new QDoubleSpinBox(&dialog);
    c3->setRange(-1000000, 1000000);
    c3->setValue(0.2);
    c3->setSingleStep(0.1);
    parametersLayout->addWidget(c3, 0, 5, 1, 1);

    label = new QLabel("Twofold astigmatism: ", &dialog);
    parametersLayout->addWidget(label, 1, 3, 1, 1);
    label = new QLabel("Value (nm): ", &dialog);
    parametersLayout->addWidget(label, 1, 4, 1, 1);
    QDoubleSpinBox* f_a2 = new QDoubleSpinBox(&dialog);
    f_a2->setRange(-1000000, 1000000);
    f_a2->setValue(0.0);
    f_a2->setSingleStep(1000);
    parametersLayout->addWidget(f_a2, 1, 5, 1, 1);
    label = new QLabel("Orientation (rad): ", &dialog);
    parametersLayout->addWidget(label, 2, 4, 1, 1);
    QDoubleSpinBox* phi_a2 = new QDoubleSpinBox(&dialog);
    phi_a2->setValue(0.0);
    phi_a2->setSingleStep(0.1);
    parametersLayout->addWidget(phi_a2, 2, 5, 1, 1);

    label = new QLabel("Threefold astigmatism: ", &dialog);
    parametersLayout->addWidget(label, 3, 3, 1, 1);
    label = new QLabel("Value (nm): ", &dialog);
    parametersLayout->addWidget(label, 3, 4, 1, 1);
    QDoubleSpinBox* f_a3 = new QDoubleSpinBox(&dialog);
    f_a3->setRange(-1000000, 1000000);
    f_a3->setValue(0.0);
    f_a3->setSingleStep(1000);
    parametersLayout->addWidget(f_a3, 3, 5, 1, 1);
    label = new QLabel("Orientation (rad): ", &dialog);
    parametersLayout->addWidget(label, 4, 4, 1, 1);
    QDoubleSpinBox* phi_a3 = new QDoubleSpinBox(&dialog);
    phi_a3->setValue(0.0);
    phi_a3->setSingleStep(0.1);
    parametersLayout->addWidget(phi_a3, 4, 5, 1, 1);

    label = new QLabel("Coma: ", &dialog);
    parametersLayout->addWidget(label, 5, 3, 1, 1);
    label = new QLabel("Value (nm): ", &dialog);
    parametersLayout->addWidget(label, 5, 4, 1, 1);
    QDoubleSpinBox* f_c3 = new QDoubleSpinBox(&dialog);
    f_c3->setRange(-1000000, 1000000);
    f_c3->setValue(1500.0);
    f_c3->setSingleStep(1000);
    parametersLayout->addWidget(f_c3, 5, 5, 1, 1);
    label = new QLabel("Orientation (rad): ", &dialog);
    parametersLayout->addWidget(label, 6, 4, 1, 1);
    QDoubleSpinBox* phi_c3 = new QDoubleSpinBox(&dialog);
    phi_c3->setValue(0.0);
    phi_c3->setSingleStep(0.1);
    parametersLayout->addWidget(phi_c3, 6, 5, 1, 1);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, &dialog);
    QObject::connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    layout->addItem(parametersLayout);
    layout->addWidget(buttons);
    dialog.setLayout(layout);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable

    // substitute values
    if (dialog.exec() == QDialog::Accepted) {
      QString pythonScript = this->Internals->scriptSource;
      pythonScript.replace("###voltage###",
                           QString("voltage = %1").arg(voltage->value()));
      pythonScript.replace("###alpha_max###",
                           QString("alpha_max = %1").arg(alpha_max->value()));
      pythonScript.replace("###Nxy###", QString("Nxy = %1").arg(Nxy->value()));
      pythonScript.replace("###Nz###", QString("Nz = %1").arg(Nz->value()));
      pythonScript.replace("###dxy###", QString("dxy = %1").arg(dxy->value()));
      pythonScript.replace("###df_min###",
                           QString("df_min = %1").arg(df_min->value()));
      pythonScript.replace("###df_max###",
                           QString("df_max = %1").arg(df_max->value()));
      pythonScript.replace("###c3###", QString("c3 = %1").arg(c3->value()));
      pythonScript.replace("###f_a2###",
                           QString("f_a2 = %1").arg(f_a2->value()));
      pythonScript.replace("###phi_a2###",
                           QString("phi_a2 = %1").arg(phi_a2->value()));
      pythonScript.replace("###f_a3###",
                           QString("f_a3 = %1").arg(f_a3->value()));
      pythonScript.replace("###phi_a3###",
                           QString("phi_a3 = %1").arg(phi_a3->value()));
      pythonScript.replace("###f_c3###",
                           QString("f_c3 = %1").arg(f_c3->value()));
      pythonScript.replace("###phi_c3###",
                           QString("phi_c3 = %1").arg(phi_c3->value()));
      generator.setScript(pythonScript);
      const int shape[3] = { Nxy->value(), Nxy->value(), Nz->value() };
      this->dataSourceAdded(generator.createDataSource(shape));
    }
  } // end of else if
}

void PythonGeneratedDatasetReaction::dataSourceAdded(
  vtkSmartPointer<vtkSMSourceProxy> proxy)
{
  if (!proxy) {
    return;
  }
  DataSource* dataSource = new DataSource(proxy);
  ModuleManager::instance().addDataSource(dataSource);

  vtkSMViewProxy* view = ActiveObjects::instance().activeView();
  // Create an outline module for the source in the active view.
  if (Module* module = ModuleManager::instance().createAndAddModule(
        "Outline", dataSource, view)) {
    ActiveObjects::instance().setActiveModule(module);
  }
  if (Module* module = ModuleManager::instance().createAndAddModule(
        "Orthogonal Slice", dataSource, view)) {
    ActiveObjects::instance().setActiveModule(module);
  }
  DataSource* previousActiveDataSource =
    ActiveObjects::instance().activeDataSource();
  if (!previousActiveDataSource) {
    pqRenderView* renderView =
      qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());
    if (renderView) {
      tomviz::createCameraOrbit(dataSource->producer(),
                                renderView->getRenderViewProxy());
    }
  }
}

vtkSmartPointer<vtkSMSourceProxy>
PythonGeneratedDatasetReaction::getSourceProxy(const QString& label,
                                               const QString& script,
                                               const int shape[3])
{
  PythonGeneratedDataSource generator(label);
  generator.setScript(script);
  return generator.createDataSource(shape);
}
}
