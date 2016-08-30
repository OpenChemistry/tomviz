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

#include <pqPropertiesPanel.h>
#include <pqProxyWidget.h>
#include <pqView.h>
#include <vtkDataSetAttributes.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <vtkAlgorithm.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkFieldData.h>
#include <vtkSMSourceProxy.h>

#include <QDebug>
#include <QDialog>
#include <QDoubleValidator>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>

namespace tomviz {

class DataPropertiesPanel::DPPInternals
{
public:
  Ui::DataPropertiesPanel Ui;
  QPointer<DataSource> CurrentDataSource;
  QPointer<pqProxyWidget> ColorMapWidget;
  QPointer<QWidget> TiltAnglesSeparator;

  DPPInternals(QWidget* parent)
  {
    Ui::DataPropertiesPanel& ui = this->Ui;
    ui.setupUi(parent);
    ui.xLengthBox->setValidator(new QDoubleValidator(ui.xLengthBox));
    ui.yLengthBox->setValidator(new QDoubleValidator(ui.yLengthBox));
    ui.zLengthBox->setValidator(new QDoubleValidator(ui.zLengthBox));
    QVBoxLayout* l = ui.verticalLayout;

    l->setSpacing(pqPropertiesPanel::suggestedVerticalSpacing());

    // add separator labels.
    QWidget* separator = pqProxyWidget::newGroupLabelWidget("Filename", parent);
    l->insertWidget(l->indexOf(ui.FileName), separator);

    separator =
      pqProxyWidget::newGroupLabelWidget("Original Dimensions & Range", parent);
    l->insertWidget(l->indexOf(ui.OriginalDataRange), separator);

    separator = pqProxyWidget::newGroupLabelWidget(
      "Transformed Dimensions & Range", parent);
    l->insertWidget(l->indexOf(ui.TransformedDataRange), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Units and Size", parent);

    l->insertWidget(l->indexOf(ui.LengthWidget), separator);

    this->TiltAnglesSeparator =
      pqProxyWidget::newGroupLabelWidget("Tilt Angles", parent);
    l->insertWidget(l->indexOf(ui.SetTiltAnglesButton),
                    this->TiltAnglesSeparator);

    this->clear();
  }

  void clear()
  {
    Ui::DataPropertiesPanel& ui = this->Ui;
    ui.FileName->setText("");
    ui.OriginalDataRange->setText("");
    ui.OriginalDataType->setText("Type:");
    ui.TransformedDataRange->setText("");
    ui.TransformedDataType->setText("Type:");
    if (this->ColorMapWidget) {
      ui.verticalLayout->removeWidget(this->ColorMapWidget);
      delete this->ColorMapWidget;
    }
    this->TiltAnglesSeparator->hide();
    ui.SetTiltAnglesButton->hide();
    ui.TiltAnglesTable->clear();
    ui.TiltAnglesTable->setRowCount(0);
    ui.TiltAnglesTable->hide();
  }

  void updateSpacing(int axis, double newLength)
  {
    if (!this->CurrentDataSource) {
      return;
    }
    int extent[6];
    double spacing[3];
    this->CurrentDataSource->getExtent(extent);
    this->CurrentDataSource->getSpacing(spacing);
    spacing[axis] = newLength / (extent[2 * axis + 1] - extent[2 * axis] + 1);
    this->CurrentDataSource->setSpacing(spacing);
  }
};

DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : Superclass(parentObject),
    Internals(new DataPropertiesPanel::DPPInternals(this))
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(setDataSource(DataSource*)));
  this->connect(this->Internals->Ui.SetTiltAnglesButton, SIGNAL(clicked()),
                SLOT(setTiltAngles()));
  this->connect(this->Internals->Ui.changeUnitsButton, SIGNAL(clicked()),
                SLOT(setUnits()));
  this->connect(this->Internals->Ui.xLengthBox, SIGNAL(editingFinished()),
                SLOT(updateXLength()));
  this->connect(this->Internals->Ui.yLengthBox, SIGNAL(editingFinished()),
                SLOT(updateYLength()));
  this->connect(this->Internals->Ui.zLengthBox, SIGNAL(editingFinished()),
                SLOT(updateZLength()));
}

DataPropertiesPanel::~DataPropertiesPanel()
{
}

void DataPropertiesPanel::paintEvent(QPaintEvent* e)
{
  this->updateData();
  QWidget::paintEvent(e);
}

void DataPropertiesPanel::setDataSource(DataSource* dsource)
{
  if (this->Internals->CurrentDataSource) {
    this->disconnect(this->Internals->CurrentDataSource);
  }
  this->Internals->CurrentDataSource = dsource;
  if (dsource) {
    this->connect(dsource, SIGNAL(dataChanged()), SLOT(scheduleUpdate()),
                  Qt::UniqueConnection);
  }
  this->scheduleUpdate();
}

namespace {

QString getDataExtentAndRangeString(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* info = proxy->GetDataInformation(0);

  QString extentString =
    QString("%1 x %2 x %3")
      .arg(info->GetExtent()[1] - info->GetExtent()[0] + 1)
      .arg(info->GetExtent()[3] - info->GetExtent()[2] + 1)
      .arg(info->GetExtent()[5] - info->GetExtent()[4] + 1);

  if (vtkPVArrayInformation* scalarInfo =
        tomviz::scalarArrayInformation(proxy)) {
    return QString("(%1)\t%2 : %3")
      .arg(extentString)
      .arg(scalarInfo->GetComponentRange(0)[0])
      .arg(scalarInfo->GetComponentRange(0)[1]);
  } else {
    return QString("(%1)\t? : ? (type: ?)").arg(extentString);
  }
}

QString getDataTypeString(vtkSMSourceProxy* proxy)
{
  if (vtkPVArrayInformation* scalarInfo =
        tomviz::scalarArrayInformation(proxy)) {
    return QString("Type: %1")
      .arg(vtkImageScalarTypeNameMacro(scalarInfo->GetDataType()));
  } else {
    return QString("Type: ?");
  }
}
}

void DataPropertiesPanel::updateData()
{
  if (!this->updateNeeded) {
    return;
  }

  this->disconnect(this->Internals->Ui.TiltAnglesTable,
                   SIGNAL(cellChanged(int, int)), this,
                   SLOT(onTiltAnglesModified(int, int)));
  this->Internals->clear();

  DataSource* dsource = this->Internals->CurrentDataSource;
  if (!dsource) {
    return;
  }
  Ui::DataPropertiesPanel& ui = this->Internals->Ui;
  ui.FileName->setText(dsource->filename());

  ui.OriginalDataRange->setText(
    getDataExtentAndRangeString(dsource->originalDataSource()));
  ui.OriginalDataType->setText(
    getDataTypeString(dsource->originalDataSource()));
  ui.TransformedDataRange->setText(
    getDataExtentAndRangeString(dsource->producer()));
  ui.TransformedDataType->setText(getDataTypeString(dsource->producer()));

  int extent[6];
  double spacing[3];
  dsource->getExtent(extent);
  dsource->getSpacing(spacing);
  ui.xLengthBox->setText(
    QString("%1").arg(spacing[0] * (extent[1] - extent[0] + 1)));
  ui.yLengthBox->setText(
    QString("%1").arg(spacing[1] * (extent[3] - extent[2] + 1)));
  ui.zLengthBox->setText(
    QString("%1").arg(spacing[2] * (extent[5] - extent[4] + 1)));

  // display tilt series data
  if (dsource->type() == DataSource::TiltSeries) {
    this->Internals->TiltAnglesSeparator->show();
    ui.SetTiltAnglesButton->show();
    ui.TiltAnglesTable->show();
    QVector<double> tiltAngles = dsource->getTiltAngles();
    ui.TiltAnglesTable->setRowCount(tiltAngles.size());
    ui.TiltAnglesTable->setColumnCount(1);
    for (int i = 0; i < tiltAngles.size(); ++i) {
      QTableWidgetItem* item = new QTableWidgetItem();
      item->setData(Qt::DisplayRole, QString::number(tiltAngles[i]));
      ui.TiltAnglesTable->setItem(i, 0, item);
    }
  } else {
    this->Internals->TiltAnglesSeparator->hide();
    ui.SetTiltAnglesButton->hide();
    ui.TiltAnglesTable->hide();
  }
  this->connect(this->Internals->Ui.TiltAnglesTable,
                SIGNAL(cellChanged(int, int)),
                SLOT(onTiltAnglesModified(int, int)));

  this->updateNeeded = false;
}

void DataPropertiesPanel::onTiltAnglesModified(int row, int column)
{
  DataSource* dsource = this->Internals->CurrentDataSource;
  // The table shouldn't be shown if this is not true, so this slot shouldn't be
  // called
  Q_ASSERT(dsource->type() == DataSource::TiltSeries);
  QTableWidgetItem* item =
    this->Internals->Ui.TiltAnglesTable->item(row, column);
  bool ok;
  double value = item->data(Qt::DisplayRole).toDouble(&ok);
  if (ok) {
    bool needToAdd = false;
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
  DataSource* dsource = this->Internals->CurrentDataSource;
  QMainWindow* mainWindow = qobject_cast<QMainWindow*>(this->window());
  SetTiltAnglesReaction::showSetTiltAnglesUI(mainWindow, dsource);
}

void DataPropertiesPanel::scheduleUpdate()
{
  this->updateNeeded = true;
  if (this->isVisible()) {
    this->updateData();
  }
}

void DataPropertiesPanel::setUnits()
{
  QDialog dialog;
  QHBoxLayout* layout = new QHBoxLayout;
  dialog.setLayout(layout);
  QLineEdit* line = new QLineEdit;
  layout->addWidget(line);
  QPushButton* okButton = new QPushButton("Ok");
  layout->addWidget(okButton);
  QObject::connect(okButton, SIGNAL(clicked()), &dialog, SLOT(accept()));
  if (dialog.exec()) {
    this->Internals->CurrentDataSource->setUnits(line->text());
  }
}

void DataPropertiesPanel::updateXLength()
{
  const QString& text = this->Internals->Ui.xLengthBox->text();
  bool ok;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse X Length string";
    return;
  }
  this->Internals->updateSpacing(0, newLength);
  this->updateData();
}

void DataPropertiesPanel::updateYLength()
{
  const QString& text = this->Internals->Ui.yLengthBox->text();
  bool ok;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse Y Length string";
    return;
  }
  this->Internals->updateSpacing(1, newLength);
  this->updateData();
}

void DataPropertiesPanel::updateZLength()
{
  const QString& text = this->Internals->Ui.zLengthBox->text();
  bool ok;
  double newLength = text.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to parse Z Length string";
    return;
  }
  this->Internals->updateSpacing(2, newLength);
  this->updateData();
}
}
