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

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "PythonUtilities.h"
#include "Utilities.h"
#include "Variant.h"

#include "pqActiveObjects.h"
#include "pqRenderView.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkSMProxyIterator.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSmartPointer.h"
#include "vtkTrivialProducer.h"

#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
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
    tomviz::Python::initialize();

    {
      tomviz::Python python;
      this->OperatorModule = python.import("tomviz.utils");
      if (!this->OperatorModule.isValid()) {
        qCritical() << "Failed to import tomviz.utils module.";
      }

      // Don't let these be the same, even for similar scripts.  Seems to
      // cause python crashes.
      QString moduleName =
        QString("tomviz_%1%2").arg(this->label).arg(number_of_scripts++);

      tomviz::Python::Module module =
        python.import(script, this->label, moduleName);
      if (!module.isValid()) {
        qCritical() << "Failed to create module.";
        return;
      }

      this->GenerateFunction = module.findFunction("generate_dataset");
      if (!this->GenerateFunction.isValid()) {
        qCritical() << "Script does not have a 'generate_dataset' function.";
        return;
      }

      this->MakeDatasetFunction =
        this->OperatorModule.findFunction("make_dataset");
      if (!this->MakeDatasetFunction.isValid()) {
        qCritical() << "Could not find make_dataset function in tomviz.utils";
        return;
      }
    }

    this->pythonScript = script;
  }

  tomviz::DataSource* createDataSource(const int shape[3])
  {
    vtkImageData* image = vtkImageData::New();

    {
      tomviz::Python python;

      tomviz::Python::Object result;
      tomviz::Python::Tuple args(5);

      args.set(0, shape[0]);
      args.set(1, shape[1]);
      args.set(2, shape[2]);
      tomviz::Python::Object imageData =
        tomviz::Python::VTK::GetObjectFromPointer(image);
      args.set(3, imageData);
      args.set(4, this->GenerateFunction);

      tomviz::Python::Dict kwargs;
      foreach (QString key, this->arguments.keys()) {
        tomviz::Variant value = tomviz::toVariant(this->arguments[key]);
        kwargs.set(key, value);
      }

      result = this->MakeDatasetFunction.call(args, kwargs);
      if (!result.isValid()) {
        qCritical() << "Failed to execute script.";
        return nullptr;
      }
    }

    QVariantMap vmap;
    foreach (QString key, this->arguments.keys()) {
      vmap[key] = this->arguments[key];
    }

    QJsonObject argsJson = QJsonObject::fromVariantMap(vmap);

    QJsonArray size;
    size.append(shape[0]);
    size.append(shape[1]);
    size.append(shape[2]);

    QJsonObject description;
    description["script"] = this->pythonScript;
    description["label"] = this->label;
    description["args"] = argsJson;
    description["shape"] = size;

    tomviz::DataSource* dataSource = new tomviz::DataSource(
      this->label, tomviz::DataSource::Volume, nullptr,
      tomviz::DataSource::PersistenceState::Transient, description);
    // setData consumes the caller's reference, so image can't be a vtkNew
    dataSource->setData(image);

    return dataSource;
  }

  void setArguments(QMap<QString, QVariant> args) { this->arguments = args; }

private:
  tomviz::Python::Module OperatorModule;
  tomviz::Python::Function GenerateFunction;
  tomviz::Python::Function MakeDatasetFunction;
  QString label;
  QString pythonScript;
  QMap<QString, QVariant> arguments;
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

    generator.setScript(this->Internals->scriptSource);
    QMap<QString, QVariant> args;
    args["CONSTANT"] = constant->value();
    generator.setArguments(args);

    int shape[3];
    shapeWidget->getShape(shape);
    LoadDataReaction::dataSourceAdded(generator.createDataSource(shape), true,
                                      false);
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
      generator.setScript(this->Internals->scriptSource);
      QMap<QString, QVariant> args;
      args["p_in"] = innerStructureParameter->value();
      args["p_s"] = shapeParameter->value();
      args["sparsity"] = sparsityParameter->value();
      generator.setArguments(args);

      int shape[3];
      shapeLayout->getShape(shape);
      LoadDataReaction::dataSourceAdded(generator.createDataSource(shape), true,
                                        false);
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
      generator.setScript(this->Internals->scriptSource);
      QMap<QString, QVariant> args;
      args["voltage"] = voltage->value();
      args["alpha_max"] = alpha_max->value();
      args["Nxy"] = Nxy->value();
      args["Nz"] = Nz->value();
      args["dxy"] = dxy->value();
      args["df_min"] = df_min->value();
      args["df_max"] = df_max->value();
      args["c3"] = c3->value();
      args["f_a2"] = f_a2->value();
      args["phi_a2"] = phi_a2->value();
      args["f_a3"] = f_a3->value();
      args["phi_a3"] = phi_a3->value();
      args["f_c3"] = f_c3->value();
      args["phi_c3"] = phi_c3->value();
      generator.setArguments(args);

      const int shape[3] = { Nxy->value(), Nxy->value(), Nz->value() };
      LoadDataReaction::dataSourceAdded(generator.createDataSource(shape), true,
                                        false);
    }
  } // end of else if
}

DataSource* PythonGeneratedDatasetReaction::createDataSource(
  const QJsonObject& sourceInformation)
{
  PythonGeneratedDataSource generator(sourceInformation["label"].toString());
  generator.setScript(sourceInformation["script"].toString());
  QVariantMap args = sourceInformation["args"].toObject().toVariantMap();
  generator.setArguments(args);
  int shape[3];
  QJsonArray shapeJson = sourceInformation["shape"].toArray();
  shape[0] = shapeJson[0].toInt();
  shape[1] = shapeJson[1].toInt();
  shape[2] = shapeJson[2].toInt();
  return generator.createDataSource(shape);
}
}
