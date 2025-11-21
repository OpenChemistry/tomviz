/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataPropertiesPanel.h"

#include "ui_DataPropertiesPanel.h"

#include <algorithm>
#include <iterator>

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ListEditorWidget.h"
#include "SetTiltAnglesOperator.h"
#include "SetTiltAnglesReaction.h"
#include "TimeSeriesStep.h"
#include "Utilities.h"

#include <pqNonEditableStyledItemDelegate.h>
#include <pqPropertiesPanel.h>
#include <pqProxyWidget.h>
#include <pqView.h>
#include <vtkDataSetAttributes.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <vtkAlgorithm.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkFieldData.h>
#include <vtkSMSourceProxy.h>

#include <QCheckBox>
#include <QClipboard>
#include <QDebug>
#include <QDoubleValidator>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QSignalBlocker>

namespace tomviz {

DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : QWidget(parentObject), m_ui(new Ui::DataPropertiesPanel)
{
  m_ui->setupUi(this);
  m_ui->xLengthBox->setValidator(new QDoubleValidator(m_ui->xLengthBox));
  m_ui->yLengthBox->setValidator(new QDoubleValidator(m_ui->yLengthBox));
  m_ui->zLengthBox->setValidator(new QDoubleValidator(m_ui->zLengthBox));
  m_ui->xVoxelSizeBox->setValidator(new QDoubleValidator(m_ui->xVoxelSizeBox));
  m_ui->yVoxelSizeBox->setValidator(new QDoubleValidator(m_ui->yVoxelSizeBox));
  m_ui->zVoxelSizeBox->setValidator(new QDoubleValidator(m_ui->zVoxelSizeBox));
  m_ui->TiltAnglesTable->installEventFilter(this);
  m_ui->ScalarsTable->setModel(&m_scalarsTableModel);

  QVBoxLayout* l = m_ui->verticalLayout;
  l->setSpacing(pqPropertiesPanel::suggestedVerticalSpacing());

  // add separator labels.
  QWidget* separator = pqProxyWidget::newGroupLabelWidget("Filename", this);
  l->insertWidget(l->indexOf(m_ui->FileName), separator);

  // add separator labels.
  separator = pqProxyWidget::newGroupLabelWidget("Active Scalars", this);
  l->insertWidget(l->indexOf(m_ui->ActiveScalars), separator);

  m_timeSeriesSeparator =
    pqProxyWidget::newGroupLabelWidget("Time Series", this);
  l->insertWidget(l->indexOf(m_ui->timeSeriesGroup), m_timeSeriesSeparator);

  separator = pqProxyWidget::newGroupLabelWidget("Dimensions & Range", this);
  l->insertWidget(l->indexOf(m_ui->DataRange), separator);

  separator = pqProxyWidget::newGroupLabelWidget("Units and Size", this);
  auto index = l->indexOf(m_ui->LengthWidget);
  l->insertWidget(index, separator);

  separator = pqProxyWidget::newGroupLabelWidget("Transformations", this);
  index = l->indexOf(m_ui->transformWidget);
  l->insertWidget(index, separator);

  m_tiltAnglesSeparator =
    pqProxyWidget::newGroupLabelWidget("Tilt Angles", this);
  l->insertWidget(l->indexOf(m_ui->SetTiltAnglesButton), m_tiltAnglesSeparator);

  clear();

  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(setDataSource(DataSource*)));
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateAxesGridLabels()));
  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          SLOT(updateAxesGridLabels()));
  connect(m_ui->SetTiltAnglesButton, SIGNAL(clicked()), SLOT(setTiltAngles()));
  connect(m_ui->unitBox, SIGNAL(editingFinished()), SLOT(updateUnits()));
  connect(m_ui->xLengthBox, &QLineEdit::editingFinished,
          [this]() { this->updateLength(m_ui->xLengthBox, 0); });
  connect(m_ui->yLengthBox, &QLineEdit::editingFinished,
          [this]() { this->updateLength(m_ui->yLengthBox, 1); });
  connect(m_ui->zLengthBox, &QLineEdit::editingFinished,
          [this]() { this->updateLength(m_ui->zLengthBox, 2); });

  connect(m_ui->xVoxelSizeBox, &QLineEdit::editingFinished,
          [this]() { this->updateVoxelSize(m_ui->xVoxelSizeBox, 0); });
  connect(m_ui->yVoxelSizeBox, &QLineEdit::editingFinished,
          [this]() { this->updateVoxelSize(m_ui->yVoxelSizeBox, 1); });
  connect(m_ui->zVoxelSizeBox, &QLineEdit::editingFinished,
          [this]() { this->updateVoxelSize(m_ui->zVoxelSizeBox, 2); });

  connect(m_ui->xOriginBox, &QLineEdit::editingFinished,
          [this]() { this->updateOrigin(m_ui->xOriginBox, 0); });
  connect(m_ui->yOriginBox, &QLineEdit::editingFinished,
          [this]() { this->updateOrigin(m_ui->yOriginBox, 1); });
  connect(m_ui->zOriginBox, &QLineEdit::editingFinished,
          [this]() { this->updateOrigin(m_ui->zOriginBox, 2); });

  connect(m_ui->xOrientationBox, &QLineEdit::editingFinished,
          [this]() { this->updateOrientation(m_ui->xOrientationBox, 0); });
  connect(m_ui->yOrientationBox, &QLineEdit::editingFinished,
          [this]() { this->updateOrientation(m_ui->yOrientationBox, 1); });
  connect(m_ui->zOrientationBox, &QLineEdit::editingFinished,
          [this]() { this->updateOrientation(m_ui->zOrientationBox, 2); });

  connect(m_ui->ActiveScalars, &QComboBox::currentTextChanged, this,
          &DataPropertiesPanel::setActiveScalars);
  connect(&m_scalarsTableModel, &DataPropertiesModel::activeScalarsChanged,
          this, &DataPropertiesPanel::setActiveScalars);
  connect(m_ui->componentNamesEditor, &ComboTextEditor::itemEdited, this,
          &DataPropertiesPanel::componentNameEdited);

  connect(m_ui->showTimeSeriesLabel, &QCheckBox::toggled,
          &ActiveObjects::instance(), &ActiveObjects::setShowTimeSeriesLabel);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::showTimeSeriesLabelChanged, this,
          &DataPropertiesPanel::updateTimeSeriesGroup);

  connect(m_ui->editTimeSeries, &QPushButton::clicked, this,
          &DataPropertiesPanel::editTimeSeries);

  connect(m_ui->interactTranslate, &QCheckBox::clicked,
          &ActiveObjects::instance(), &ActiveObjects::enableTranslation);
  connect(m_ui->interactRotate, &QCheckBox::clicked, &ActiveObjects::instance(),
          &ActiveObjects::enableRotation);
  connect(m_ui->interactScale, &QCheckBox::clicked, &ActiveObjects::instance(),
          &ActiveObjects::enableScaling);

  connect(&ActiveObjects::instance(), &ActiveObjects::translationStateChanged,
          m_ui->interactTranslate, &QCheckBox::setChecked);
  connect(&ActiveObjects::instance(), &ActiveObjects::rotationStateChanged,
          m_ui->interactRotate, &QCheckBox::setChecked);
  connect(&ActiveObjects::instance(), &ActiveObjects::scalingStateChanged,
          m_ui->interactScale, &QCheckBox::setChecked);

  m_ui->interactTranslate->setChecked(
    ActiveObjects::instance().translationEnabled());
  m_ui->interactRotate->setChecked(ActiveObjects::instance().rotationEnabled());
  m_ui->interactScale->setChecked(ActiveObjects::instance().scalingEnabled());
}

DataPropertiesPanel::~DataPropertiesPanel() {}

void DataPropertiesPanel::paintEvent(QPaintEvent* e)
{
  updateData();
  QWidget::paintEvent(e);
}

void DataPropertiesPanel::setDataSource(DataSource* dsource)
{
  if (m_currentDataSource) {
    disconnect(m_currentDataSource, 0, this, 0);
    disconnect(&m_scalarsTableModel, 0, m_currentDataSource, 0);
  }
  m_currentDataSource = dsource;
  if (dsource) {
    connect(dsource, SIGNAL(dataChanged()), SLOT(scheduleUpdate()),
            Qt::UniqueConnection);
    connect(dsource, &DataSource::dataPropertiesChanged, this,
            &DataPropertiesPanel::onDataPropertiesChanged);
    connect(dsource, &DataSource::displayPositionChanged, this,
            &DataPropertiesPanel::onDataPositionChanged);
    connect(dsource, &DataSource::displayOrientationChanged, this,
            &DataPropertiesPanel::onDataOrientationChanged);
    connect(&m_scalarsTableModel, &DataPropertiesModel::scalarsRenamed, dsource,
            &DataSource::renameScalarsArray);
  }
  m_scalarIndexes.clear();
  scheduleUpdate();
}

void DataPropertiesPanel::onDataPropertiesChanged()
{
  if (!m_currentDataSource) {
    return;
  }

  double length[3];
  m_currentDataSource->getPhysicalDimensions(length);
  auto origin = m_currentDataSource->displayPosition();
  auto orientation = m_currentDataSource->displayOrientation();
  auto* spacing = m_currentDataSource->getSpacing();

  m_ui->xLengthBox->setText(QString("%1").arg(length[0]));
  m_ui->yLengthBox->setText(QString("%1").arg(length[1]));
  m_ui->zLengthBox->setText(QString("%1").arg(length[2]));

  m_ui->xVoxelSizeBox->setText(QString("%1").arg(spacing[0]));
  m_ui->yVoxelSizeBox->setText(QString("%1").arg(spacing[1]));
  m_ui->zVoxelSizeBox->setText(QString("%1").arg(spacing[2]));

  m_ui->xOriginBox->setText(QString("%1").arg(origin[0]));
  m_ui->yOriginBox->setText(QString("%1").arg(origin[1]));
  m_ui->zOriginBox->setText(QString("%1").arg(origin[2]));

  m_ui->xOrientationBox->setText(QString("%1").arg(orientation[0]));
  m_ui->yOrientationBox->setText(QString("%1").arg(orientation[1]));
  m_ui->zOrientationBox->setText(QString("%1").arg(orientation[2]));
}

void DataPropertiesPanel::onDataPositionChanged(double, double, double)
{
  this->onDataPropertiesChanged();
}

void DataPropertiesPanel::onDataOrientationChanged(double, double, double)
{
  this->onDataPropertiesChanged();
}

namespace {

QString getDataDimensionsString(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* info = proxy->GetDataInformation(0);

  QString extentString =
    QString("Dimensions: %1 x %2 x %3")
      .arg(info->GetExtent()[1] - info->GetExtent()[0] + 1)
      .arg(info->GetExtent()[3] - info->GetExtent()[2] + 1)
      .arg(info->GetExtent()[5] - info->GetExtent()[4] + 1);

  return extentString;
}

QString getNumVoxelsString(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* info = proxy->GetDataInformation(0);
  vtkTypeInt64 numVoxels = info->GetNumberOfPoints();
  return "Voxels: " + getSizeNearestThousand(numVoxels);
}

QString getMemSizeString(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* info = proxy->GetDataInformation(0);

  // GetMemorySize() returns kilobytes
  // Cast it to size_t to prevent integer overflows
  size_t memSize = static_cast<size_t>(info->GetMemorySize()) * 1000;

  return "Memory: " + getSizeNearestThousand(memSize, true);
}

} // namespace

QList<ArrayInfo> DataPropertiesPanel::getArraysInfo(DataSource* dataSource)
{
  QList<ArrayInfo> arraysInfo;

  // If we don't have the scalar indexes, we sort the names and then save the
  // indexes, these will be used to preserve the displayed order even after
  // a rename.
  if (m_scalarIndexes.isEmpty()) {
    auto scalars = dataSource->listScalars();
    auto sortedScalars = scalars;

    // sort the scalars names
    std::sort(sortedScalars.begin(), sortedScalars.end(),
              [](const QString& a, const QString& b) { return a < b; });

    // Now save the indexes of the sorted scalars
    for (auto scalar : sortedScalars) {
      auto index = std::distance(
        scalars.begin(), std::find(scalars.begin(), scalars.end(), scalar));

      m_scalarIndexes.push_back(index);
    }
  }

  // Remove any invalid scalar indices to prevent a crash
  QList<int> toRemove;
  for (auto i : m_scalarIndexes) {
    // name, type, data range, data type, active
    auto arrayName = dataSource->scalarsName(i);
    auto array = dataSource->getScalarsArray(arrayName);
    if (!array) {
      toRemove.append(i);
    }
  }

  while (toRemove.size() != 0) {
    m_scalarIndexes.remove(toRemove[0]);
    toRemove.removeAt(0);
  }

  for (auto i : m_scalarIndexes) {
    // name, type, data range, data type, active
    auto arrayName = dataSource->scalarsName(i);
    auto array = dataSource->getScalarsArray(arrayName);
    if (!array) {
      continue;
    }

    QString dataType = vtkImageScalarTypeNameMacro(array->GetDataType());
    int numComponents = array->GetNumberOfComponents();
    QString dataRange;
    double range[2];
    bool active = m_currentDataSource->activeScalars() == arrayName;

    for (int j = 0; j < numComponents; j++) {
      if (j != 0) {
        dataRange.append(", ");
      }
      array->GetRange(range, j);
      QString componentRange = QString("[%1, %2]").arg(range[0]).arg(range[1]);
      dataRange.append(componentRange);
    }

    arraysInfo.push_back(ArrayInfo(arrayName,
                                   dataType == "string" ? tr("NA") : dataRange,
                                   dataType, active));
  }

  return arraysInfo;
}

void DataPropertiesPanel::updateActiveScalarsCombo(
  QComboBox* scalarsCombo, const QList<ArrayInfo>& arraysInfo)
{
  scalarsCombo->clear();
  scalarsCombo->blockSignals(true);

  foreach (auto array, arraysInfo) {
    scalarsCombo->addItem(array.name);
    if (array.active) {
      scalarsCombo->setCurrentText(array.name);
    }
  }

  scalarsCombo->blockSignals(false);
}

void DataPropertiesPanel::updateInformationWidget(
  QTableView* scalarsTable, const QList<ArrayInfo>& arraysInfo)
{
  auto model = static_cast<DataPropertiesModel*>(scalarsTable->model());
  model->setArraysInfo(arraysInfo);
  scalarsTable->resizeColumnsToContents();
  scalarsTable->horizontalHeader()->setSectionResizeMode(1,
                                                         QHeaderView::Stretch);
}

void DataPropertiesPanel::updateData()
{
  if (!m_updateNeeded) {
    return;
  }

  disconnect(m_ui->TiltAnglesTable, SIGNAL(cellChanged(int, int)), this,
             SLOT(onTiltAnglesModified(int, int)));
  clear();

  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }

  m_ui->FileName->setText(dsource->fileName());

  m_ui->DataRange->setText(getDataDimensionsString(dsource->proxy()));
  m_ui->NumVoxels->setText(getNumVoxelsString(dsource->proxy()));
  m_ui->MemSize->setText(getMemSizeString(dsource->proxy()));

  this->onDataPropertiesChanged();

  m_ui->unitBox->setText(m_currentDataSource->getUnits());

  auto sourceProxy = vtkSMSourceProxy::SafeDownCast(dsource->proxy());
  if (sourceProxy) {
    auto arraysInfo = getArraysInfo(dsource);
    updateInformationWidget(m_ui->ScalarsTable, arraysInfo);
    updateActiveScalarsCombo(m_ui->ActiveScalars, arraysInfo);
  }

  // display tilt series data
  if (dsource->type() == DataSource::TiltSeries) {
    m_tiltAnglesSeparator->show();
    m_ui->SetTiltAnglesButton->show();
    m_ui->TiltAnglesTable->show();
    QVector<double> tiltAngles = dsource->getTiltAngles();
    m_ui->TiltAnglesTable->setRowCount(tiltAngles.size());
    m_ui->TiltAnglesTable->setColumnCount(1);
    for (int i = 0; i < tiltAngles.size(); ++i) {
      QTableWidgetItem* item = new QTableWidgetItem();
      item->setData(Qt::DisplayRole, QString::number(tiltAngles[i]));
      m_ui->TiltAnglesTable->setItem(i, 0, item);
    }
  } else {
    m_tiltAnglesSeparator->hide();
    m_ui->SetTiltAnglesButton->hide();
    m_ui->TiltAnglesTable->hide();
  }
  connect(m_ui->TiltAnglesTable, SIGNAL(cellChanged(int, int)),
          SLOT(onTiltAnglesModified(int, int)));

  updateTimeSeriesGroup();
  updateComponentsCombo();

  m_updateNeeded = false;
}

void DataPropertiesPanel::updateTimeSeriesGroup()
{
  auto* ds = m_currentDataSource.data();
  bool visible = ds && ds->hasTimeSteps();

  m_timeSeriesSeparator->setVisible(visible);
  m_ui->timeSeriesGroup->setVisible(visible);

  if (!visible) {
    return;
  }

  m_ui->showTimeSeriesLabel->setChecked(
    ActiveObjects::instance().showTimeSeriesLabel());
}

void DataPropertiesPanel::editTimeSeries()
{
  if (m_timeSeriesEditor) {
    m_timeSeriesEditor->reject();
    m_timeSeriesEditor->deleteLater();
  }

  QPointer<DataSource> ds = m_currentDataSource;
  if (!ds) {
    return;
  }

  auto steps = ds->timeSeriesSteps();
  QStringList labels;
  for (auto& step : steps) {
    labels.append(step.label);
  }

  m_timeSeriesEditor = new ListEditorDialog(labels, this);
  m_timeSeriesEditor->setToolTip("Left-click and drag a row to re-order time "
                                 "series steps. Double-click to edit a label.");
  m_timeSeriesEditor->setWindowTitle("Edit Time Series");
  m_timeSeriesEditor->show();
  auto* editor = m_timeSeriesEditor.data();

  auto onAccepted = [ds, editor, steps]() {
    auto newOrder = editor->currentOrder();
    auto newNames = editor->currentNames();

    // First, re-arrange the time steps according to the new ordering
    QList<TimeSeriesStep> newSteps;
    for (auto i : newOrder) {
      newSteps.append(steps[i]);
    }

    // Now, rename them
    for (int i = 0; i < newNames.size(); ++i) {
      newSteps[i].label = newNames[i];
    }

    // Finally, set the new steps
    ds->setTimeSeriesSteps(newSteps);
  };

  connect(m_timeSeriesEditor, &QDialog::accepted, ds, onAccepted);
}

void DataPropertiesPanel::updateComponentsCombo()
{
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }

  auto label = m_ui->componentNamesEditorLabel;
  auto combo = m_ui->componentNamesEditor;
  auto blocked = QSignalBlocker(combo);

  // Only make this editor visible if there is more than one component
  bool visible = dsource->scalars()->GetNumberOfComponents() > 1;
  label->setVisible(visible);
  combo->setVisible(visible);

  if (visible) {
    combo->clear();
    combo->addItems(dsource->componentNames());
  }
}

void DataPropertiesPanel::onTiltAnglesModified(int row, int column)
{
  DataSource* dsource = m_currentDataSource;
  // The table shouldn't be shown if this is not true, so this slot shouldn't be
  // called
  Q_ASSERT(dsource->type() == DataSource::TiltSeries);
  QTableWidgetItem* item = m_ui->TiltAnglesTable->item(row, column);
  auto ok = false;
  auto value = item->data(Qt::DisplayRole).toDouble(&ok);
  if (ok) {
    auto needToAdd = false;
    SetTiltAnglesOperator* op = nullptr;
    if (dsource->operators().size() > 0) {
      op = qobject_cast<SetTiltAnglesOperator*>(dsource->operators().last());
    }
    if (!op) {
      op = new SetTiltAnglesOperator;
      op->setParent(dsource);
      needToAdd = true;
    }
    auto tiltAngles = op->tiltAngles();
    tiltAngles[row] = value;
    op->setTiltAngles(tiltAngles);
    if (needToAdd) {
      dsource->addOperator(op);
    }
  } else {
    std::cerr << "Invalid tilt angle." << std::endl;
  }
}

bool DataPropertiesPanel::eventFilter(QObject* obj, QEvent* event)
{
  QKeyEvent* ke = dynamic_cast<QKeyEvent*>(event);
  if (ke && obj == m_ui->TiltAnglesTable) {
    if (ke->matches(QKeySequence::Paste) && ke->type() == QEvent::KeyPress) {
      QClipboard* clipboard = QGuiApplication::clipboard();
      const QMimeData* mimeData = clipboard->mimeData();
      if (mimeData->hasText()) {
        QString text = mimeData->text();
        QStringList rows = text.split("\n");
        QStringList angles;
        for (const QString& row : rows) {
          angles << row.split("\t")[0];
        }
        auto ranges = m_ui->TiltAnglesTable->selectedRanges();
        // check if the table in the clipboard is of numbers
        for (const QString& angle : angles) {
          bool ok;
          angle.toDouble(&ok);
          if (!ok) {
            QMessageBox::warning(
              this, "Error",
              QString("Error: pasted tilt angle %1 is not a number")
                .arg(angle));
            return true;
          }
        }
        // If separate blocks of rows selected, cancel the paste
        // since we don't know where to put angles
        if (ranges.size() != 1) {
          QMessageBox::warning(
            this, "Error",
            "Pasting is not supported with non-continuous selections");
          return true;
        }
        // If multiple rows selected and it is not equal to
        // the number of angles pasted, cancel the paste
        if (ranges[0].rowCount() > 1 && ranges[0].rowCount() != angles.size()) {
          QMessageBox::warning(
            this, "Error",
            QString("Cells selected (%1) does not match number "
                    "of angles to paste (%2).  \n"
                    "Please select one cell to mark the start "
                    "location for pasting or select the same "
                    "number of cells that will be pasted into.")
              .arg(ranges[0].rowCount())
              .arg(angles.size()));
          return true;
        }
        auto needToAdd = false;
        SetTiltAnglesOperator* op = nullptr;
        DataSource* dsource = m_currentDataSource;
        if (dsource->operators().size() > 0) {
          op =
            qobject_cast<SetTiltAnglesOperator*>(dsource->operators().last());
        }
        if (!op) {
          op = new SetTiltAnglesOperator;
          op->setParent(dsource);
          needToAdd = true;
        }
        auto tiltAngles = op->tiltAngles();
        int startRow = ranges[0].topRow();
        for (int i = 0; i < angles.size() && i + startRow < tiltAngles.size();
             ++i) {
          bool ok;
          tiltAngles[i + startRow] = angles[i].toDouble(
            &ok); // no need to check ok, we checked these above
        }
        op->setTiltAngles(tiltAngles);
        if (needToAdd) {
          dsource->addOperator(op);
        }
      }
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void DataPropertiesPanel::setTiltAngles()
{
  DataSource* dsource = m_currentDataSource;
  auto mainWindow = qobject_cast<QMainWindow*>(window());
  SetTiltAnglesReaction::showSetTiltAnglesUI(mainWindow, dsource);
}

void DataPropertiesPanel::scheduleUpdate()
{
  m_updateNeeded = true;
  if (isVisible()) {
    updateData();
  }
}

void DataPropertiesPanel::updateUnits()
{
  if (m_currentDataSource) {
    const QString& text = m_ui->unitBox->text();
    m_currentDataSource->setUnits(text);
    updateAxesGridLabels();
  }
}

bool DataPropertiesPanel::parseField(QLineEdit* widget, double& value)
{
  const QString& text = widget->text();
  bool ok = false;
  value = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse string";
  }

  return ok;
}

void DataPropertiesPanel::updateLength(QLineEdit* widget, int axis)
{
  double newLength;
  bool ok = DataPropertiesPanel::parseField(widget, newLength);

  if (!ok) {
    return;
  }

  updateSpacing(axis, newLength);
  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  resetCamera();
  emit dsource->dataPropertiesChanged();
}

void DataPropertiesPanel::updateVoxelSize(QLineEdit* widget, int axis)
{
  double newSize;
  bool ok = DataPropertiesPanel::parseField(widget, newSize);

  if (!ok) {
    return;
  }

  double spacing[3];
  m_currentDataSource->getSpacing(spacing);
  spacing[axis] = newSize;
  m_currentDataSource->setSpacing(spacing);

  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  resetCamera();
  emit dsource->dataPropertiesChanged();
}

void DataPropertiesPanel::updateOrigin(QLineEdit* widget, int axis)
{
  double newValue;
  bool ok = DataPropertiesPanel::parseField(widget, newValue);

  if (!ok) {
    return;
  }

  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }

  auto origin = dsource->displayPosition();
  double newOrigin[3] = { origin[0], origin[1], origin[2] };
  newOrigin[axis] = newValue;
  dsource->setDisplayPosition(newOrigin);
}

void DataPropertiesPanel::updateOrientation(QLineEdit* widget, int axis)
{
  double newValue;
  bool ok = DataPropertiesPanel::parseField(widget, newValue);

  if (!ok) {
    return;
  }

  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }

  auto orientation = dsource->displayOrientation();
  double newOrientation[3] = { orientation[0], orientation[1], orientation[2] };
  newOrientation[axis] = newValue;
  dsource->setDisplayOrientation(newOrientation);
}

void DataPropertiesPanel::resetCamera()
{
  pqView* view = ActiveObjects::instance().activePqView();
  if (view) {
    view->resetDisplay();
  }
}

void DataPropertiesPanel::updateAxesGridLabels()
{
  vtkSMViewProxy* view = ActiveObjects::instance().activeView();
  if (!view) {
    return;
  }
  auto axesGrid = vtkSMPropertyHelper(view, "AxesGrid", true).GetAsProxy();
  DataSource* ds = ActiveObjects::instance().activeDataSource();
  if (!axesGrid || !ds) {
    return;
  }
  QString xTitle = QString("X (%1)").arg(ds->getUnits());
  QString yTitle = QString("Y (%1)").arg(ds->getUnits());
  QString zTitle = QString("Z (%1)").arg(ds->getUnits());
  vtkSMPropertyHelper(axesGrid, "XTitle").Set(xTitle.toUtf8().data());
  vtkSMPropertyHelper(axesGrid, "YTitle").Set(yTitle.toUtf8().data());
  vtkSMPropertyHelper(axesGrid, "ZTitle").Set(zTitle.toUtf8().data());
  axesGrid->UpdateVTKObjects();

  pqView* qtView =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (qtView) {
    qtView->render();
  }
}

void DataPropertiesPanel::setActiveScalars(const QString& activeScalars)
{
  if (activeScalars.size() == 0) {
    return;
  }

  if (m_currentDataSource &&
      m_currentDataSource->activeScalars() != activeScalars) {
    m_currentDataSource->setActiveScalars(activeScalars);
  }
}

void DataPropertiesPanel::componentNameEdited(int index, const QString& name)
{
  if (!m_currentDataSource) {
    return;
  }

  m_currentDataSource->setComponentName(index, name);
}

void DataPropertiesPanel::clear()
{
  m_ui->FileName->setText("");
  m_ui->DataRange->setText("");
  m_ui->NumVoxels->setText("");
  m_ui->MemSize->setText("");
  m_ui->ActiveScalars->clear();
  m_scalarsTableModel.setArraysInfo(QList<ArrayInfo>());

  if (m_colorMapWidget) {
    m_ui->verticalLayout->removeWidget(m_colorMapWidget);
    delete m_colorMapWidget;
  }
  m_tiltAnglesSeparator->hide();
  m_ui->SetTiltAnglesButton->hide();
  m_ui->TiltAnglesTable->clear();
  m_ui->TiltAnglesTable->setRowCount(0);
  m_ui->TiltAnglesTable->hide();
}

void DataPropertiesPanel::updateSpacing(int axis, double newLength)
{
  if (!m_currentDataSource) {
    return;
  }
  int extent[6];
  double spacing[3];
  m_currentDataSource->getExtent(extent);
  m_currentDataSource->getSpacing(spacing);
  spacing[axis] = newLength / (extent[2 * axis + 1] - extent[2 * axis] + 1);
  m_currentDataSource->setSpacing(spacing);
}
} // namespace tomviz
