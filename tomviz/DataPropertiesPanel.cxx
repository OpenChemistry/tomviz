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
#include "DataPropertiesPanel.h"
#include "ui_DataPropertiesPanel.h"

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

#include <QDebug>
#include <QDoubleValidator>
#include <QMainWindow>

namespace tomviz {

DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : QWidget(parentObject), m_ui(new Ui::DataPropertiesPanel)
{
  m_ui->setupUi(this);
  m_ui->xLengthBox->setValidator(new QDoubleValidator(m_ui->xLengthBox));
  m_ui->yLengthBox->setValidator(new QDoubleValidator(m_ui->yLengthBox));
  m_ui->zLengthBox->setValidator(new QDoubleValidator(m_ui->zLengthBox));

  QVBoxLayout* l = m_ui->verticalLayout;
  l->setSpacing(pqPropertiesPanel::suggestedVerticalSpacing());

  // add separator labels.
  QWidget* separator = pqProxyWidget::newGroupLabelWidget("Filename", this);
  l->insertWidget(l->indexOf(m_ui->FileName), separator);

  separator =
    pqProxyWidget::newGroupLabelWidget("Original Dimensions & Range", this);
  l->insertWidget(l->indexOf(m_ui->OriginalDataRange), separator);

  separator =
    pqProxyWidget::newGroupLabelWidget("Transformed Dimensions & Range", this);
  l->insertWidget(l->indexOf(m_ui->TransformedDataRange), separator);

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
}

DataPropertiesPanel::~DataPropertiesPanel()
{
}

void DataPropertiesPanel::paintEvent(QPaintEvent* e)
{
  updateData();
  QWidget::paintEvent(e);
}

void DataPropertiesPanel::setDataSource(DataSource* dsource)
{
  if (m_currentDataSource) {
    disconnect(m_currentDataSource);
  }
  m_currentDataSource = dsource;
  if (dsource) {
    connect(dsource, SIGNAL(dataChanged()), SLOT(scheduleUpdate()),
            Qt::UniqueConnection);
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

} // namespace

void DataPropertiesPanel::updateInformationWidget(
  QTreeWidget* infoTreeWidget, vtkPVDataInformation* dataInfo)
{
  infoTreeWidget->clear();

  vtkPVDataSetAttributesInformation* pointDataInfo =
    dataInfo->GetPointDataInformation();
  if (pointDataInfo) {
    QPixmap pointDataPixmap(":/pqWidgets/Icons/pqPointData16.png");
    int numArrays = pointDataInfo->GetNumberOfArrays();
    for (int i = 0; i < numArrays; i++) {
      vtkPVArrayInformation* arrayInfo;
      arrayInfo = pointDataInfo->GetArrayInformation(i);
      // name, type, data range, data type
      QTreeWidgetItem* item = new QTreeWidgetItem(infoTreeWidget);
      item->setData(0, Qt::DisplayRole, arrayInfo->GetName());
      QString dataType = vtkImageScalarTypeNameMacro(arrayInfo->GetDataType());
      item->setData(2, Qt::DisplayRole, dataType);
      int numComponents = arrayInfo->GetNumberOfComponents();
      QString dataRange;
      double range[2];
      for (int j = 0; j < numComponents; j++) {
        if (j != 0) {
          dataRange.append(", ");
        }
        arrayInfo->GetComponentRange(j, range);
        QString componentRange =
          QString("[%1, %2]").arg(range[0]).arg(range[1]);
        dataRange.append(componentRange);
      }
      item->setData(1, Qt::DisplayRole,
                    dataType == "string" ? tr("NA") : dataRange);
      item->setData(1, Qt::ToolTipRole, dataRange);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      if (arrayInfo->GetIsPartial()) {
        item->setForeground(0, QBrush(QColor("darkBlue")));
        item->setData(0, Qt::DisplayRole,
                      QString("%1 (partial)").arg(arrayInfo->GetName()));
      } else {
        item->setForeground(0, QBrush(QColor("darkGreen")));
      }
    }
  }

  infoTreeWidget->header()->resizeSections(QHeaderView::ResizeToContents);
  infoTreeWidget->setItemDelegate(new pqNonEditableStyledItemDelegate(this));
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

  m_ui->FileName->setText(dsource->filename());

  m_ui->OriginalDataRange->setText(
    getDataDimensionsString(dsource->originalDataSource()));
  m_ui->TransformedDataRange->setText(
    getDataDimensionsString(dsource->producer()));

  int extent[6];
  double spacing[3];
  dsource->getExtent(extent);
  dsource->getSpacing(spacing);
  m_ui->xLengthBox->setText(
    QString("%1").arg(spacing[0] * (extent[1] - extent[0])));
  m_ui->yLengthBox->setText(
    QString("%1").arg(spacing[1] * (extent[3] - extent[2])));
  m_ui->zLengthBox->setText(
    QString("%1").arg(spacing[2] * (extent[5] - extent[4])));
  m_ui->unitBox->setText(m_currentDataSource->getUnits(0));

  vtkSMSourceProxy* sourceProxy =
    vtkSMSourceProxy::SafeDownCast(dsource->originalDataSource());
  if (sourceProxy) {
    updateInformationWidget(m_ui->OriginalDataTreeWidget,
                            sourceProxy->GetDataInformation());
  }

  sourceProxy = vtkSMSourceProxy::SafeDownCast(dsource->producer());
  if (sourceProxy) {
    updateInformationWidget(m_ui->TransformedDataTreeWidget,
                            sourceProxy->GetDataInformation());
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
  emit dsource->dataPropertiesChanged();
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
  QString xTitle = QString("X (%1)").arg(ds->getUnits(0));
  QString yTitle = QString("Y (%1)").arg(ds->getUnits(1));
  QString zTitle = QString("Z (%1)").arg(ds->getUnits(2));
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

void DataPropertiesPanel::clear()
{
  m_ui->FileName->setText("");
  m_ui->OriginalDataRange->setText("");
  m_ui->OriginalDataTreeWidget->clear();
  m_ui->TransformedDataRange->setText("");
  m_ui->TransformedDataTreeWidget->clear();
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
  spacing[axis] = newLength / (extent[2 * axis + 1] - extent[2 * axis]);
  m_currentDataSource->setSpacing(spacing);
}
}
