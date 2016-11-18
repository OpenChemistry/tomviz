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
#include "AddPythonTransformReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "InterfaceBuilder.h"
#include "OperatorDialog.h"
#include "OperatorPython.h"
#include "SelectVolumeWidget.h"
#include "SpinBox.h"

#include <pqCoreUtilities.h>
#include <vtkImageData.h>
#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cassert>

namespace {

class SelectSliceRangeWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  SelectSliceRangeWidget(int* ext, bool showAxisSelector = true,
                         QWidget* p = nullptr)
    : Superclass(p), firstSlice(new tomviz::SpinBox(this)),
      lastSlice(new tomviz::SpinBox(this)), axisSelect(new QComboBox(this))
  {
    for (int i = 0; i < 6; ++i) {
      this->Extent[i] = ext[i];
    }
    QHBoxLayout* rangeLayout = new QHBoxLayout;
    QLabel* startLabel = new QLabel("Start:", this);
    QLabel* endLabel = new QLabel("End:", this);

    this->firstSlice->setRange(0, ext[5] - ext[4]);
    this->firstSlice->setValue(0);
    this->lastSlice->setRange(0, ext[5] - ext[4]);
    this->lastSlice->setValue(0);

    rangeLayout->addWidget(startLabel);
    rangeLayout->addWidget(this->firstSlice);
    rangeLayout->addWidget(endLabel);
    rangeLayout->addWidget(this->lastSlice);

    QHBoxLayout* axisSelectLayout = new QHBoxLayout;
    QLabel* axisLabel = new QLabel("Axis:", this);
    this->axisSelect->addItem("X");
    this->axisSelect->addItem("Y");
    this->axisSelect->addItem("Z");
    this->axisSelect->setCurrentIndex(2);

    axisSelectLayout->addWidget(axisLabel);
    axisSelectLayout->addWidget(this->axisSelect);

    QVBoxLayout* widgetLayout = new QVBoxLayout;
    widgetLayout->addLayout(rangeLayout);
    widgetLayout->addLayout(axisSelectLayout);

    if (!showAxisSelector) {
      axisLabel->hide();
      this->axisSelect->hide();
    }

    this->setLayout(widgetLayout);

    QObject::connect(this->firstSlice, SIGNAL(editingFinished()), this,
                     SLOT(onMinimumChanged()));
    QObject::connect(this->lastSlice, SIGNAL(editingFinished()), this,
                     SLOT(onMaximumChanged()));
    this->connect(axisSelect, SIGNAL(currentIndexChanged(int)),
                  SLOT(onAxisChanged(int)));
  }

  ~SelectSliceRangeWidget() {}

  int startSlice() const { return this->firstSlice->value(); }

  int endSlice() const { return this->lastSlice->value(); }

  int axis() const { return this->axisSelect->currentIndex(); }

public slots:
  void onMinimumChanged()
  {
    int val = this->firstSlice->value();
    if (this->lastSlice->value() < val) {
      this->lastSlice->setValue(val);
    }
  }

  void onMaximumChanged()
  {
    int val = this->lastSlice->value();
    if (this->firstSlice->value() > val) {
      this->firstSlice->setValue(val);
    }
  }

  void onAxisChanged(int newAxis)
  {
    int newSliceMax = this->Extent[2 * newAxis + 1] - this->Extent[2 * newAxis];
    this->firstSlice->setMaximum(newSliceMax);
    this->lastSlice->setMaximum(newSliceMax);
  }

private:
  Q_DISABLE_COPY(SelectSliceRangeWidget)
  tomviz::SpinBox* firstSlice; // delete slices starting at this slice index
  tomviz::SpinBox* lastSlice;  // delete slices ending at this slice index
  QComboBox* axisSelect;
  int Extent[6];
};
}

#include "AddPythonTransformReaction.moc"

namespace tomviz {

AddPythonTransformReaction::AddPythonTransformReaction(QAction* parentObject,
                                                       const QString& l,
                                                       const QString& s,
                                                       bool rts, bool rv,
                                                       const QString& json)
  : Superclass(parentObject), jsonSource(json), scriptLabel(l), scriptSource(s),
    interactive(false), requiresTiltSeries(rts), requiresVolume(rv)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

AddPythonTransformReaction::~AddPythonTransformReaction()
{
}

void AddPythonTransformReaction::updateEnableState()
{
  bool enable = ActiveObjects::instance().activeDataSource() != nullptr;
  if (enable && this->requiresTiltSeries) {
    enable = ActiveObjects::instance().activeDataSource()->type() ==
             DataSource::TiltSeries;
  }
  if (enable && this->requiresVolume) {
    enable = ActiveObjects::instance().activeDataSource()->type() ==
             DataSource::Volume;
  }
  parentAction()->setEnabled(enable);
}

OperatorPython* AddPythonTransformReaction::addExpression(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source) {
    return nullptr;
  }

  bool hasJson = this->jsonSource.size() > 0;
  if (hasJson) {
    // Use JSON to build the interface via the OperatorDialog
    OperatorDialog dialog(pqCoreUtilities::mainWidget());
    dialog.setWindowTitle(scriptLabel);
    dialog.setJSONDescription(this->jsonSource);

    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> parameterValues = dialog.values();
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        parameterValues, jsonSource);
    }
    // Handle transforms with custom UIs
  } else if (scriptLabel == "Shift Volume") {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int* extent = data->GetExtent();

    QDialog dialog(pqCoreUtilities::mainWidget());
    dialog.setWindowTitle("Shift Volume");

    QHBoxLayout* layout = new QHBoxLayout;
    QLabel* label = new QLabel("Shift to apply:", &dialog);
    layout->addWidget(label);
    QSpinBox* spinx = new QSpinBox(&dialog);
    spinx->setRange(-(extent[1] - extent[0]), extent[1] - extent[0]);
    spinx->setValue(0);
    QSpinBox* spiny = new QSpinBox(&dialog);
    spiny->setRange(-(extent[3] - extent[2]), extent[3] - extent[2]);
    spiny->setValue(0);
    QSpinBox* spinz = new QSpinBox(&dialog);
    spinz->setRange(-(extent[5] - extent[4]), extent[5] - extent[4]);
    spinz->setValue(0);
    layout->addWidget(spinx);
    layout->addWidget(spiny);
    layout->addWidget(spinz);
    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    v->addLayout(layout);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable

    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      QList<QVariant> values;
      values << spinx->value() << spiny->value() << spinz->value();
      arguments.insert("SHIFT", values);

      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Remove Bad Pixels") {
    QDialog dialog(pqCoreUtilities::mainWidget());
    dialog.setWindowTitle("Remove Bad Pixels");
    QHBoxLayout* layout = new QHBoxLayout;
    QLabel* label = new QLabel("Remove bad pixels that are ", &dialog);
    layout->addWidget(label);
    QDoubleSpinBox* threshold = new QDoubleSpinBox(&dialog);
    threshold->setMinimum(0);
    threshold->setValue(5);
    layout->addWidget(threshold);
    label = new QLabel("times local standard deviation from local median.");
    layout->addWidget(label);

    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    v->addLayout(layout);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable

    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      arguments.insert("threshold", threshold->value());
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Crop") {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int* extent = data->GetExtent();

    QDialog dialog(pqCoreUtilities::mainWidget());
    QHBoxLayout* layout1 = new QHBoxLayout;
    QLabel* label = new QLabel("Crop data start:", &dialog);
    layout1->addWidget(label);
    QSpinBox* spinx = new QSpinBox(&dialog);
    spinx->setRange(extent[0], extent[1]);
    spinx->setValue(extent[0]);
    QSpinBox* spiny = new QSpinBox(&dialog);
    spiny->setRange(extent[2], extent[3]);
    spiny->setValue(extent[2]);
    QSpinBox* spinz = new QSpinBox(&dialog);
    spinz->setRange(extent[4], extent[5]);
    spinz->setValue(extent[4]);
    layout1->addWidget(label);
    layout1->addWidget(spinx);
    layout1->addWidget(spiny);
    layout1->addWidget(spinz);
    QHBoxLayout* layout2 = new QHBoxLayout;
    label = new QLabel("Crop data end:", &dialog);
    layout2->addWidget(label);
    QSpinBox* spinxx = new QSpinBox(&dialog);
    spinxx->setRange(extent[0], extent[1]);
    spinxx->setValue(extent[1]);
    QSpinBox* spinyy = new QSpinBox(&dialog);
    spinyy->setRange(extent[2], extent[3]);
    spinyy->setValue(extent[3]);
    QSpinBox* spinzz = new QSpinBox(&dialog);
    spinzz->setRange(extent[4], extent[5]);
    spinzz->setValue(extent[5]);
    layout2->addWidget(label);
    layout2->addWidget(spinxx);
    layout2->addWidget(spinyy);
    layout2->addWidget(spinzz);
    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    v->addLayout(layout1);
    v->addLayout(layout2);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable
    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      QList<QVariant> values;
      values << spinx->value() << spiny->value() << spinz->value();
      arguments.insert("START_CROP", values);
      values.clear();

      values << spinxx->value() << spinyy->value() << spinzz->value();
      arguments.insert("END_CROP", values);
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Delete Slices") {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int* shape = data->GetExtent();

    QDialog dialog(pqCoreUtilities::mainWidget());

    SelectSliceRangeWidget* sliceRange =
      new SelectSliceRangeWidget(shape, true, &dialog);

    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    v->addWidget(sliceRange);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.setWindowTitle("Delete Slices");
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable

    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      arguments.insert("firstSlice", sliceRange->startSlice());
      arguments.insert("lastSlice", sliceRange->endSlice());
      arguments.insert("axis", sliceRange->axis());
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Reconstruct (Direct Fourier)") {
    QDialog dialog(pqCoreUtilities::mainWidget());
    dialog.setWindowTitle("Direct Fourier Reconstruction");

    QGridLayout* layout = new QGridLayout;
    // Description
    QLabel* label = new QLabel(
      "Reconstruct a tilt series using Direct Fourier Method (DFM). \n"
      "The tilt axis must be parallel to the x-direction and centered in the "
      "y-direction.\n"
      "The size of reconstruction will be (Nx,Ny,Ny).\n"
      "Reconstrucing a 512x512x512 tomogram typically takes 30-40 seconds.");
    label->setWordWrap(true);
    layout->addWidget(label, 0, 0, 1, 2);

    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    v->addLayout(layout);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable
    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Reconstruct (Back Projection)") {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int* extent = data->GetExtent();

    QDialog dialog(pqCoreUtilities::mainWidget());
    dialog.setWindowTitle("Weighted Back Projection Reconstruction");

    QGridLayout* layout = new QGridLayout;
    // Description
    QLabel* label = new QLabel(
      "Reconstruct a tilt series using Weighted Back Projection (WBP) method. "
      "\n"
      "The tilt axis must be parallel to the x-direction and centered in the "
      "y-direction.\n"
      "The size of reconstruction will be (Nx,N,N), where Nx is the number of "
      "pixels in"
      " x-direction and N can be specified below. The maximum N allowed is "
      "4096.\n"
      "Reconstrucing a 512x512x512 tomogram typically takes 7-10 mins.");
    label->setWordWrap(true);
    layout->addWidget(label, 0, 0, 1, 2);

    label = new QLabel("Reconstruction Size (N):");
    layout->addWidget(label, 1, 0, 1, 1);

    QSpinBox* reconSize = new QSpinBox;
    reconSize->setMaximum(4096);
    reconSize->setValue(extent[3] - extent[2] + 1);
    layout->addWidget(reconSize, 1, 1, 1, 1);

    label = new QLabel("Fourier Weighting Filter:");
    layout->addWidget(label, 2, 0, 1, 1);

    QComboBox* filters = new QComboBox(&dialog);
    filters->addItem("None");
    filters->addItem("Ramp");
    filters->addItem("Shepp-Logan");
    filters->addItem("Cosine");
    filters->addItem("Hamming");
    filters->addItem("Hann");
    filters->setCurrentIndex(1); // Default filter: ramp
    layout->addWidget(filters, 2, 1, 1, 1);

    label = new QLabel("Back Projection Interpolation Method:");
    layout->addWidget(label, 3, 0, 1, 1);
    QComboBox* interpMethods = new QComboBox(&dialog);
    interpMethods->addItem("Linear");
    interpMethods->addItem("Nearest");
    interpMethods->addItem("Spline");
    interpMethods->addItem("Cubic");
    interpMethods->setCurrentIndex(0); // Default filter: linear
    layout->addWidget(interpMethods, 3, 1, 1, 1);

    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    v->addLayout(layout);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable
    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      arguments.insert("Nrecon", reconSize->value());
      arguments.insert("filter", filters->currentIndex());
      arguments.insert("interp", interpMethods->currentIndex());
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Reconstruct (SIRT)") {
    QDialog dialog(pqCoreUtilities::mainWidget());
    dialog.setWindowTitle("SIRT Reconstruction");

    QGridLayout* layout = new QGridLayout;
    // Description
    QLabel* label =
      new QLabel("Reconstruct a tilt series using Simultaneous Iterative "
                 "Reconstruction Techniques Technique (SIRT). \n"
                 "The tilt axis must be parallel to the x-direction and "
                 "centered in the y-direction.\n"
                 "The size of reconstruction will be (Nx,Ny,Ny). The number of "
                 "iterations can be specified below.\n"
                 "Reconstrucing a 256x256x256 tomogram typically takes more "
                 "than 100 mins with 5 iterations.");
    label->setWordWrap(true);
    layout->addWidget(label, 0, 0, 1, 2);

    label = new QLabel("Number of Iterations:");
    layout->addWidget(label, 1, 0, 1, 1);

    QSpinBox* Niter = new QSpinBox;
    Niter->setValue(10);
    Niter->setMinimum(1);

    layout->addWidget(Niter, 1, 1, 1, 1);
    label = new QLabel("Update step size:");
    layout->addWidget(label, 2, 0, 1, 1);

    QDoubleSpinBox* updateStepSize = new QDoubleSpinBox;
    updateStepSize->setDecimals(5);
    updateStepSize->setValue(0.0001);
    updateStepSize->setMinimum(0);
    updateStepSize->setSingleStep(0.0001);
    layout->addWidget(updateStepSize, 2, 1, 1, 1);

    label = new QLabel("Update method:");
    layout->addWidget(label, 3, 0, 1, 1);

    QComboBox* updateMethod = new QComboBox(&dialog);
    updateMethod->addItem("Landweber");
    updateMethod->addItem("Cimmino");
    updateMethod->addItem("Component average");
    updateMethod->setCurrentIndex(0); // Default filter: ramp
    layout->addWidget(updateMethod, 3, 1, 1, 1);

    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    v->addLayout(layout);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable
    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      arguments.insert("Niter", Niter->value());
      arguments.insert("stepSize", updateStepSize->value());
      arguments.insert("updateMethodIndex", updateMethod->currentIndex());
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Reconstruct (Constraint-based Direct Fourier)") {
    QDialog dialog(pqCoreUtilities::mainWidget());
    dialog.setWindowTitle("Constraint-based Direct Fourier Reconstruction");

    QGridLayout* layout = new QGridLayout;
    // Description
    QLabel* label = new QLabel(
      "Reconstruct a tilt series using constraint-based Direct Fourier method. "
      "The tilt axis must be parallel to the x-direction and centered in the "
      "y-direction. The size of reconstruction will be (Nx,Ny,Ny).\n"
      "Reconstrucing a 512x512x512 tomogram typically takes xxxx mins.");
    label->setWordWrap(true);
    layout->addWidget(label, 0, 0, 1, 3);

    label = new QLabel("Number of iterations:");
    layout->addWidget(label, 1, 0, 1, 1);

    QSpinBox* Niter = new QSpinBox;
    Niter->setRange(0, 9999);
    Niter->setSingleStep(10);
    Niter->setValue(100);
    layout->addWidget(Niter, 1, 1, 1, 1);

    label = new QLabel("Update support constraint every");
    layout->addWidget(label, 2, 0, 1, 1);
    QSpinBox* supportUpdateIteration = new QSpinBox;
    supportUpdateIteration->setRange(0, 9999);
    supportUpdateIteration->setSingleStep(10);
    supportUpdateIteration->setValue(50);
    layout->addWidget(supportUpdateIteration, 2, 1, 1, 1);
    label = new QLabel("iteration using the \"Shrink Wrap\" method:");
    layout->addWidget(label, 2, 2, 1, 1);

    label = new QLabel("1. Apply a Fourier Gaussian filter with sigma = "
                       "maximum spatial frequency *");
    layout->addWidget(label, 3, 0, 1, 2);
    QDoubleSpinBox* supportSigma = new QDoubleSpinBox;
    supportSigma->setRange(0, 1);
    supportSigma->setValue(0.1);
    supportSigma->setSingleStep(0.1);
    layout->addWidget(supportSigma, 3, 2, 1, 1);

    label = new QLabel("2. Threshold tomogram by setting");
    layout->addWidget(label, 4, 0, 1, 1);

    QDoubleSpinBox* supportThreshold = new QDoubleSpinBox;
    supportThreshold->setRange(0, 100);
    supportThreshold->setValue(10);
    supportThreshold->setSingleStep(5);
    layout->addWidget(supportThreshold, 4, 1, 1, 1);

    label = new QLabel("% smallest voxels to zero");
    layout->addWidget(label, 4, 2, 1, 1);

    QVBoxLayout* v = new QVBoxLayout;
    QDialogButtonBox* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    v->addLayout(layout);
    v->addWidget(buttons);
    dialog.setLayout(v);
    dialog.layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable
    if (dialog.exec() == QDialog::Accepted) {
      QMap<QString, QVariant> arguments;
      arguments.insert("Niter", Niter->value());
      arguments.insert("Niter_update_support", supportUpdateIteration->value());
      arguments.insert("supportSigma", supportSigma->value());
      arguments.insert("supportThreshold", supportThreshold->value());
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments);
    }
  } else if (scriptLabel == "Clear Volume") {
    QDialog* dialog = new QDialog(pqCoreUtilities::mainWidget());
    dialog->setWindowTitle("Select Volume to Clear");
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    double origin[3];
    double spacing[3];
    int extent[6];
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    image->GetOrigin(origin);
    image->GetSpacing(spacing);
    image->GetExtent(extent);

    QVBoxLayout* layout = new QVBoxLayout();
    SelectVolumeWidget* selectionWidget = new SelectVolumeWidget(
      origin, spacing, extent, extent, source->displayPosition(), dialog);
    QObject::connect(source, &DataSource::displayPositionChanged,
                     selectionWidget, &SelectVolumeWidget::dataMoved);
    QDialogButtonBox* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), dialog, SLOT(reject()));
    layout->addWidget(selectionWidget);
    layout->addWidget(buttons);
    dialog->setLayout(layout);

    this->connect(dialog, SIGNAL(accepted()),
                  SLOT(addExpressionFromNonModalDialog()));
    dialog->show();
    dialog->layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable

  } else if (scriptLabel == "Background Subtraction (Manual)") {
    QDialog* dialog = new QDialog(pqCoreUtilities::mainWidget());
    dialog->setWindowTitle("Background Subtraction (Manual)");
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    double origin[3];
    double spacing[3];
    int extent[6];

    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    image->GetOrigin(origin);
    image->GetSpacing(spacing);
    image->GetExtent(extent);
    // Default background region
    int currentVolume[6];
    currentVolume[0] = 10;
    currentVolume[1] = 50;
    currentVolume[2] = 10;
    currentVolume[3] = 50;
    currentVolume[4] = extent[4];
    currentVolume[5] = extent[5];

    QVBoxLayout* layout = new QVBoxLayout();

    QLabel* label = new QLabel(
      "Subtract background in each image of a tilt series dataset. Specify the "
      "background regions using the x,y,z ranges or graphically in the "
      "visualization"
      " window. The mean value in the background window will be subtracted "
      "from each"
      " image tilt (x-y) in the stack's range (z).");
    label->setWordWrap(true);
    layout->addWidget(label);

    SelectVolumeWidget* selectionWidget =
      new SelectVolumeWidget(origin, spacing, extent, currentVolume,
                             source->displayPosition(), dialog);
    QObject::connect(source, &DataSource::displayPositionChanged,
                     selectionWidget, &SelectVolumeWidget::dataMoved);
    QDialogButtonBox* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), dialog, SLOT(reject()));
    layout->addWidget(selectionWidget);
    layout->addWidget(buttons);
    dialog->setLayout(layout);
    dialog->layout()->setSizeConstraint(
      QLayout::SetFixedSize); // Make the UI non-resizeable

    this->connect(dialog, SIGNAL(accepted()),
                  SLOT(addExpressionFromNonModalDialog()));
    dialog->show();
  } else {
    OperatorPython* opPython = new OperatorPython();
    opPython->setLabel(scriptLabel);
    opPython->setScript(scriptSource);

    if (interactive) {
      // Create a non-modal dialog, delete it once it has been closed.
      EditOperatorDialog* dialog = new EditOperatorDialog(
        opPython, source, true, pqCoreUtilities::mainWidget());
      dialog->setAttribute(Qt::WA_DeleteOnClose, true);
      dialog->show();
    } else {
      source->addOperator(opPython);
    }
  }
  return nullptr;
}

void AddPythonTransformReaction::addExpressionFromNonModalDialog()
{
  DataSource* source = ActiveObjects::instance().activeDataSource();
  QDialog* dialog = qobject_cast<QDialog*>(this->sender());
  if (this->scriptLabel == "Clear Volume") {
    QLayout* layout = dialog->layout();
    SelectVolumeWidget* volumeWidget = nullptr;
    for (int i = 0; i < layout->count(); ++i) {
      if ((volumeWidget =
             qobject_cast<SelectVolumeWidget*>(layout->itemAt(i)->widget()))) {
        break;
      }
    }

    assert(volumeWidget);
    int selection_extent[6];
    volumeWidget->getExtentOfSelection(selection_extent);

    int image_extent[6];
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    image->GetExtent(image_extent);

    // The image extent is not necessarily zero-based.  The numpy array is.
    // Also adding 1 to the maximum extents since numpy expects the max to be
    // one
    // past the last item of interest.
    int indices[6];
    indices[0] = selection_extent[0] - image_extent[0];
    indices[1] = selection_extent[1] - image_extent[0] + 1;
    indices[2] = selection_extent[2] - image_extent[2];
    indices[3] = selection_extent[3] - image_extent[2] + 1;
    indices[4] = selection_extent[4] - image_extent[4];
    indices[5] = selection_extent[5] - image_extent[4] + 1;

    QMap<QString, QVariant> arguments;
    QList<QVariant> range;
    range << indices[0] << indices[1];
    arguments.insert("XRANGE", range);
    range.clear();

    range << indices[2] << indices[3];
    arguments.insert("YRANGE", range);
    range.clear();

    range << indices[4] << indices[5];
    arguments.insert("ZRANGE", range);
    addPythonOperator(source, this->scriptLabel, this->scriptSource, arguments);
  }
  if (this->scriptLabel == "Background Subtraction (Manual)") {
    QLayout* layout = dialog->layout();
    SelectVolumeWidget* volumeWidget = nullptr;
    for (int i = 0; i < layout->count(); ++i) {
      if ((volumeWidget =
             qobject_cast<SelectVolumeWidget*>(layout->itemAt(i)->widget()))) {
        break;
      }
    }

    assert(volumeWidget);
    int selection_extent[6];
    volumeWidget->getExtentOfSelection(selection_extent);

    int image_extent[6];
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    image->GetExtent(image_extent);
    int indices[6];
    indices[0] = selection_extent[0] - image_extent[0];
    indices[1] = selection_extent[1] - image_extent[0] + 1;
    indices[2] = selection_extent[2] - image_extent[2];
    indices[3] = selection_extent[3] - image_extent[2] + 1;
    indices[4] = selection_extent[4] - image_extent[4];
    indices[5] = selection_extent[5] - image_extent[4] + 1;

    QMap<QString, QVariant> arguments;
    QList<QVariant> range;
    range << indices[0] << indices[1];
    arguments.insert("XRANGE", range);
    range.clear();

    range << indices[2] << indices[3];
    arguments.insert("YRANGE", range);
    range.clear();

    range << indices[4] << indices[5];
    arguments.insert("ZRANGE", range);

    addPythonOperator(source, this->scriptLabel, this->scriptSource, arguments);
  }
}

void AddPythonTransformReaction::addPythonOperator(
  DataSource* source, const QString& scriptLabel,
  const QString& scriptBaseString, const QMap<QString, QVariant> arguments,
  const QString& jsonString)
{
  // Create and add the operator
  OperatorPython* opPython = new OperatorPython();
  opPython->setJSONDescription(jsonString);
  opPython->setLabel(scriptLabel);
  opPython->setScript(scriptBaseString);
  foreach (QString key, arguments.keys()) {
    opPython->addArgument(key, arguments.value(key));
  }
  source->addOperator(opPython);
}
}
