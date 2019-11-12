/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AddPythonTransformReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "OperatorDialog.h"
#include "OperatorFactory.h"
#include "OperatorPython.h"
#include "Pipeline.h"
#include "PipelineManager.h"
#include "SelectVolumeWidget.h"
#include "SpinBox.h"
#include "Utilities.h"

#include <vtkImageData.h>
#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtDebug>

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
} // namespace

#include "AddPythonTransformReaction.moc"

namespace tomviz {

AddPythonTransformReaction::AddPythonTransformReaction(
  QAction* parentObject, const QString& l, const QString& s, bool rts, bool rv,
  bool rf, const QString& json)
  : pqReaction(parentObject), jsonSource(json), scriptLabel(l), scriptSource(s),
    interactive(false), requiresTiltSeries(rts), requiresVolume(rv),
    requiresFib(rf)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  connect(&PipelineManager::instance(), &PipelineManager::executionModeUpdated,
          this, &AddPythonTransformReaction::updateEnableState);

  // If we have JSON, check whether the operator is compatible with being run
  // in an external pipeline.
  if (!json.isEmpty()) {
    auto document = QJsonDocument::fromJson(json.toLatin1());
    if (!document.isObject()) {
      qCritical() << "Failed to parse operator JSON";
      qCritical() << json;
      return;
    } else {
      QJsonObject root = document.object();
      QJsonValueRef externalNode = root["externalCompatible"];
      if (!externalNode.isUndefined() && !externalNode.isNull()) {
        this->externalCompatible = externalNode.toBool();
      }
    }
  }

  OperatorFactory::instance().registerPythonOperator(l, s, rts, rv, rf, json);

  updateEnableState();
}

void AddPythonTransformReaction::updateEnableState()
{
  auto pipeline = ActiveObjects::instance().activePipeline();
  bool enable = pipeline != nullptr;

  if (enable) {
    auto dataSource = pipeline->transformedDataSource();

    auto executionModeCompatible =
      ((PipelineManager::instance().executionMode() ==
          Pipeline::ExecutionMode::Docker &&
        this->externalCompatible) ||
       (PipelineManager::instance().executionMode() !=
        Pipeline::ExecutionMode::Docker));

    if (((requiresTiltSeries && dataSource->type() == DataSource::TiltSeries) ||
         (requiresVolume && dataSource->type() == DataSource::Volume) ||
         (requiresFib && dataSource->type() == DataSource::FIB) ||
         (!requiresTiltSeries && !requiresVolume && !requiresFib)) &&
        executionModeCompatible) {
      enable = true;
    } else {
      enable = false;
    }
  }

  parentAction()->setEnabled(enable);
}

OperatorPython* AddPythonTransformReaction::addExpression(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeParentDataSource();
  if (!source) {
    return nullptr;
  }

  bool hasJson = this->jsonSource.size() > 0;
  if (hasJson) {
    OperatorPython* opPython = new OperatorPython(source);
    opPython->setJSONDescription(jsonSource);
    opPython->setLabel(scriptLabel);
    opPython->setScript(scriptSource);

    // Use JSON to build the interface via the EditOperatorDialog
    // If the operator doesn't have parameters, don't show the dialog on first
    // execution
    if (opPython->numberOfParameters() > 0) {
      auto dialog =
        new EditOperatorDialog(opPython, source, true, tomviz::mainWidget());
      dialog->setAttribute(Qt::WA_DeleteOnClose);
      dialog->setWindowTitle(QString("Edit %1").arg(opPython->label()));
      dialog->show();
    } else {
      source->addOperator(opPython);
    }

    // Handle transforms with custom UIs
  } else if (scriptLabel == "Shift Volume") {
    auto t = source->producer();
    auto data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int* extent = data->GetExtent();

    QDialog dialog(tomviz::mainWidget());
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
      QMap<QString, QString> typeInfo;
      typeInfo["SHIFT"] = "int";

      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments, typeInfo);
    }
  } else if (scriptLabel == "Remove Bad Pixels") {
    QDialog dialog(tomviz::mainWidget());
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
      QMap<QString, QString> typeInfo;
      typeInfo["threshold"] = "double";
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments, typeInfo);
    }
  } else if (scriptLabel == "Crop") {
    auto t = source->producer();
    auto data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int* extent = data->GetExtent();

    QDialog dialog(tomviz::mainWidget());
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
      QMap<QString, QString> typeInfo;
      typeInfo["START_CROP"] = "int";
      typeInfo["END_CROP"] = "int";
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments, typeInfo);
    }
  } else if (scriptLabel == "Delete Slices") {
    auto t = source->producer();
    auto data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int* shape = data->GetExtent();

    QDialog dialog(tomviz::mainWidget());

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
      QMap<QString, QString> typeInfo;
      typeInfo["firstSlice"] = "int";
      typeInfo["lastSlice"] = "int";
      typeInfo["axis"] = "int";
      addPythonOperator(source, this->scriptLabel, this->scriptSource,
                        arguments, typeInfo);
    }
  } else if (scriptLabel == "Clear Volume") {
    QDialog* dialog = new QDialog(tomviz::mainWidget());
    dialog->setWindowTitle("Select Volume to Clear");
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    double origin[3];
    double spacing[3];
    int extent[6];
    auto t = source->producer();
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
    QDialog* dialog = new QDialog(tomviz::mainWidget());
    dialog->setWindowTitle("Background Subtraction (Manual)");
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    double origin[3];
    double spacing[3];
    int extent[6];

    auto t = source->producer();
    auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
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
    OperatorPython* opPython = new OperatorPython(source);
    opPython->setLabel(scriptLabel);
    opPython->setScript(scriptSource);

    if (interactive) {
      // Create a non-modal dialog, delete it once it has been closed.
      auto dialog =
        new EditOperatorDialog(opPython, source, true, tomviz::mainWidget());
      dialog->setAttribute(Qt::WA_DeleteOnClose, true);
      dialog->show();
      connect(opPython, SIGNAL(destroyed()), dialog, SIGNAL(reject()));
    } else {
      source->addOperator(opPython);
    }
  }
  return nullptr;
}

void AddPythonTransformReaction::addExpressionFromNonModalDialog()
{
  auto addRanges = [](QMap<QString, QVariant>& arguments,
                      QMap<QString, QString>& typeInfo, int* indices) {
    QList<QVariant> range;
    range << indices[0] << indices[1];
    arguments.insert("XRANGE", range);
    range.clear();

    range << indices[2] << indices[3];
    arguments.insert("YRANGE", range);
    range.clear();

    range << indices[4] << indices[5];
    arguments.insert("ZRANGE", range);

    typeInfo.insert("XRANGE", "int");
    typeInfo.insert("YRANGE", "int");
    typeInfo.insert("ZRANGE", "int");
  };

  DataSource* source = ActiveObjects::instance().activeParentDataSource();
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
    auto t = source->producer();
    auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
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
    QMap<QString, QString> typeInfo;
    addRanges(arguments, typeInfo, indices);
    addPythonOperator(source, this->scriptLabel, this->scriptSource, arguments,
                      typeInfo);
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
    auto t = source->producer();
    auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    image->GetExtent(image_extent);
    int indices[6];
    indices[0] = selection_extent[0] - image_extent[0];
    indices[1] = selection_extent[1] - image_extent[0] + 1;
    indices[2] = selection_extent[2] - image_extent[2];
    indices[3] = selection_extent[3] - image_extent[2] + 1;
    indices[4] = selection_extent[4] - image_extent[4];
    indices[5] = selection_extent[5] - image_extent[4] + 1;

    QMap<QString, QVariant> arguments;
    QMap<QString, QString> typeInfo;
    addRanges(arguments, typeInfo, indices);
    addPythonOperator(source, this->scriptLabel, this->scriptSource, arguments,
                      typeInfo);
  }
}

void AddPythonTransformReaction::addPythonOperator(
  DataSource* source, const QString& scriptLabel,
  const QString& scriptBaseString, const QMap<QString, QVariant> arguments,
  const QString& jsonString)
{
  // Create and add the operator
  OperatorPython* opPython = new OperatorPython(source);
  opPython->setJSONDescription(jsonString);
  opPython->setLabel(scriptLabel);
  opPython->setScript(scriptBaseString);
  opPython->setArguments(arguments);

  source->addOperator(opPython);
}

void AddPythonTransformReaction::addPythonOperator(
  DataSource* source, const QString& scriptLabel,
  const QString& scriptBaseString, const QMap<QString, QVariant> arguments,
  const QMap<QString, QString> typeInfo)
{
  // Create and add the operator
  OperatorPython* opPython = new OperatorPython(source);
  opPython->setLabel(scriptLabel);
  opPython->setScript(scriptBaseString);
  opPython->setArguments(arguments);
  opPython->setTypeInfo(typeInfo);

  source->addOperator(opPython);
}
} // namespace tomviz
