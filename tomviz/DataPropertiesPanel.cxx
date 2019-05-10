/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataPropertiesPanel.h"

#include "ui_DataPropertiesPanel.h"

#include <algorithm>

#include "ActiveObjects.h"
#include "DataSource.h"
#include "SetTiltAnglesOperator.h"
#include "SetTiltAnglesReaction.h"
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

#include <QClipboard>
#include <QDebug>
#include <QDoubleValidator>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>

namespace tomviz {

DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : QWidget(parentObject), m_ui(new Ui::DataPropertiesPanel)
{
  m_ui->setupUi(this);
  m_ui->xLengthBox->setValidator(new QDoubleValidator(m_ui->xLengthBox));
  m_ui->yLengthBox->setValidator(new QDoubleValidator(m_ui->yLengthBox));
  m_ui->zLengthBox->setValidator(new QDoubleValidator(m_ui->zLengthBox));
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

  separator = pqProxyWidget::newGroupLabelWidget("Dimensions & Range", this);
  l->insertWidget(l->indexOf(m_ui->DataRange), separator);

  separator = pqProxyWidget::newGroupLabelWidget("Units and Size", this);

  l->insertWidget(l->indexOf(m_ui->LengthWidget), separator);

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
  connect(m_ui->xLengthBox, SIGNAL(editingFinished()), SLOT(updateXLength()));
  connect(m_ui->yLengthBox, SIGNAL(editingFinished()), SLOT(updateYLength()));
  connect(m_ui->zLengthBox, SIGNAL(editingFinished()), SLOT(updateZLength()));
  connect(m_ui->ActiveScalars, &QComboBox::currentTextChanged, this,
          &DataPropertiesPanel::setActiveScalars);
  connect(&m_scalarsTableModel, &DataPropertiesModel::activeScalarsChanged,
          this, &DataPropertiesPanel::setActiveScalars);
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
    connect(&m_scalarsTableModel, &DataPropertiesModel::scalarsRenamed, dsource,
            &DataSource::renameScalarsArray);
  }
  scheduleUpdate();
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

template <typename T>
QString getSizeNearestThousand(T num, bool labelAsBytes = false)
{
  char format = 'f';
  int prec = 1;

  QString ret;
  if (num < 1e3)
    ret = QString::number(num) + " ";
  else if (num < 1e6)
    ret = QString::number(num / 1e3, format, prec) + " K";
  else if (num < 1e9)
    ret = QString::number(num / 1e6, format, prec) + " M";
  else if (num < 1e12)
    ret = QString::number(num / 1e9, format, prec) + " G";
  else
    ret = QString::number(num / 1e12, format, prec) + " T";

  if (labelAsBytes)
    ret += "B";

  return ret;
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
  size_t memSize = info->GetMemorySize() * 1000;

  return "Memory: " + getSizeNearestThousand(memSize, true);
}

} // namespace

QList<ArrayInfo> DataPropertiesPanel::getArraysInfo(
  vtkPVDataInformation* dataInfo) const
{
  QList<ArrayInfo> arraysInfo;
  vtkPVDataSetAttributesInformation* pointDataInfo =
    dataInfo->GetPointDataInformation();
  if (pointDataInfo) {
    QList<ArrayInfo> arraysInfo_;
    int numArrays = pointDataInfo->GetNumberOfArrays();
    QList<QPair<vtkDataArray*, int>> sortMap;
    for (int i = 0; i < numArrays; i++) {
      vtkPVArrayInformation* arrayInfo;
      arrayInfo = pointDataInfo->GetArrayInformation(i);

      // name, type, data range, data type, active
      auto arrayName = arrayInfo->GetName();
      sortMap.push_back(QPair<vtkDataArray*, int>(
        m_currentDataSource->getScalarsArray(arrayName), i));
      QString dataType = vtkImageScalarTypeNameMacro(arrayInfo->GetDataType());
      int numComponents = arrayInfo->GetNumberOfComponents();
      QString dataRange;
      double range[2];
      bool active = m_currentDataSource->activeScalars() == arrayName;

      for (int j = 0; j < numComponents; j++) {
        if (j != 0) {
          dataRange.append(", ");
        }
        arrayInfo->GetComponentRange(j, range);
        QString componentRange =
          QString("[%1, %2]").arg(range[0]).arg(range[1]);
        dataRange.append(componentRange);
      }

      arraysInfo_.push_back(
        ArrayInfo(arrayName, dataType == "string" ? tr("NA") : dataRange,
                  dataType, active));
    }

    // Ensure scalars are displayed in the same order even after renaming
    std::sort(sortMap.begin(), sortMap.end(),
              [](QPair<vtkDataArray*, int> a, QPair<vtkDataArray*, int> b) {
                return a.first < b.first;
              });
    foreach (auto pair, sortMap) {
      arraysInfo.push_back(arraysInfo_[pair.second]);
    }
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

  int extent[6];
  double spacing[3];
  dsource->getExtent(extent);
  dsource->getSpacing(spacing);
  m_ui->xLengthBox->setText(
    QString("%1").arg(spacing[0] * (extent[1] - extent[0] + 1)));
  m_ui->yLengthBox->setText(
    QString("%1").arg(spacing[1] * (extent[3] - extent[2] + 1)));
  m_ui->zLengthBox->setText(
    QString("%1").arg(spacing[2] * (extent[5] - extent[4] + 1)));
  m_ui->unitBox->setText(m_currentDataSource->getUnits());

  auto sourceProxy = vtkSMSourceProxy::SafeDownCast(dsource->proxy());
  if (sourceProxy) {
    auto arraysInfo = getArraysInfo(sourceProxy->GetDataInformation());
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

  m_updateNeeded = false;
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

void DataPropertiesPanel::updateXLength()
{
  const QString& text = m_ui->xLengthBox->text();
  auto ok = false;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse X Length string";
    return;
  }
  updateSpacing(0, newLength);
  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  resetCamera();
  emit dsource->dataPropertiesChanged();
}

void DataPropertiesPanel::updateYLength()
{
  const QString& text = m_ui->yLengthBox->text();
  auto ok = false;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse Y Length string";
    return;
  }
  updateSpacing(1, newLength);
  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  resetCamera();
  emit dsource->dataPropertiesChanged();
}

void DataPropertiesPanel::updateZLength()
{
  const QString& text = m_ui->zLengthBox->text();
  auto ok = false;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse Z Length string";
    return;
  }
  updateSpacing(2, newLength);
  updateData();
  DataSource* dsource = m_currentDataSource;
  if (!dsource) {
    return;
  }
  resetCamera();
  emit dsource->dataPropertiesChanged();
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
