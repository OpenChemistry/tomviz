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
#include "SetTiltAnglesReaction.h"
#include "Utilities.h"

#include <pqPropertiesPanel.h>
#include <pqProxyWidget.h>
#include <vtkDataSetAttributes.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <pqView.h>

#include <vtkSMSourceProxy.h>
#include <vtkDataObject.h>
#include <vtkFieldData.h>
#include <vtkDataArray.h>
#include <vtkAlgorithm.h>

#include <QPointer>
#include <QPushButton>
#include <QMainWindow>

namespace tomviz
{

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
    QVBoxLayout* l = ui.verticalLayout;

    l->setSpacing(pqPropertiesPanel::suggestedVerticalSpacing());

    // add separator labels.
    QWidget* separator = pqProxyWidget::newGroupLabelWidget("Filename", parent);
    l->insertWidget(l->indexOf(ui.FileName), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Original Dimensions & Range",
                                                   parent);
    l->insertWidget(l->indexOf(ui.OriginalDataRange), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Transformed Dimensions & Range",
                                                   parent);
    l->insertWidget(l->indexOf(ui.TransformedDataRange), separator);

    this->TiltAnglesSeparator =
      pqProxyWidget::newGroupLabelWidget("Tilt Angles", parent);
    l->insertWidget(l->indexOf(ui.SetTiltAnglesButton), this->TiltAnglesSeparator);

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
    if (this->ColorMapWidget)
    {
      ui.verticalLayout->removeWidget(this->ColorMapWidget);
      delete this->ColorMapWidget;
    }
    this->TiltAnglesSeparator->hide();
    ui.SetTiltAnglesButton->hide();
    ui.TiltAnglesTable->clear();
    ui.TiltAnglesTable->setRowCount(0);
    ui.TiltAnglesTable->hide();
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
}

DataPropertiesPanel::~DataPropertiesPanel()
{
}

void DataPropertiesPanel::paintEvent(QPaintEvent *e)
{
  this->updateData();
  QWidget::paintEvent(e);
}

void DataPropertiesPanel::setDataSource(DataSource* dsource)
{
  if (this->Internals->CurrentDataSource)
  {
    this->disconnect(this->Internals->CurrentDataSource);
  }
  this->Internals->CurrentDataSource = dsource;
  if (dsource)
  {
    this->connect(dsource, SIGNAL(dataChanged()), SLOT(scheduleUpdate()),
                  Qt::UniqueConnection);
  }
  this->updateNeeded = true;
}

namespace {

QString getDataExtentAndRangeString(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* info = proxy->GetDataInformation(0);

  QString extentString = QString("%1 x %2 x %3")
                          .arg(info->GetExtent()[1] - info->GetExtent()[0] + 1)
                          .arg(info->GetExtent()[3] - info->GetExtent()[2] + 1)
                          .arg(info->GetExtent()[5] - info->GetExtent()[4] + 1);

  if (vtkPVArrayInformation* scalarInfo = tomviz::scalarArrayInformation(proxy))
  {
    return QString("(%1)\t%2 : %3").arg(extentString)
             .arg(scalarInfo->GetComponentRange(0)[0])
             .arg(scalarInfo->GetComponentRange(0)[1]);
  }
  else
  {
    return QString("(%1)\t? : ? (type: ?)").arg(extentString);
  }
}

QString getDataTypeString(vtkSMSourceProxy* proxy)
{
  if (vtkPVArrayInformation* scalarInfo = tomviz::scalarArrayInformation(proxy))
  {
    return QString("Type: %1").arg(vtkImageScalarTypeNameMacro(
                                     scalarInfo->GetDataType()));
  }
  else
  {
    return QString("Type: ?");
  }
}

}

void DataPropertiesPanel::updateData()
{
  if (!this->updateNeeded)
  {
    return;
  }

  this->disconnect(this->Internals->Ui.TiltAnglesTable,
      SIGNAL(cellChanged(int, int)), this,
      SLOT(onTiltAnglesModified(int, int)));
  this->Internals->clear();

  DataSource* dsource = this->Internals->CurrentDataSource;
  if (!dsource)
  {
    return;
  }
  Ui::DataPropertiesPanel& ui = this->Internals->Ui;
  ui.FileName->setText(dsource->filename());

  ui.OriginalDataRange->setText(getDataExtentAndRangeString(
        dsource->originalDataSource()));
  ui.OriginalDataType->setText(getDataTypeString(
        dsource->originalDataSource()));
  ui.TransformedDataRange->setText(getDataExtentAndRangeString(
        dsource->producer()));
  ui.TransformedDataType->setText(getDataTypeString(
        dsource->producer()));

  // display tilt series data
  if (dsource->type() == DataSource::TiltSeries)
  {
    this->Internals->TiltAnglesSeparator->show();
    ui.SetTiltAnglesButton->show();
    ui.TiltAnglesTable->show();
    QVector<double> tiltAngles = dsource->getTiltAngles();
    ui.TiltAnglesTable->setRowCount(tiltAngles.size());
    ui.TiltAnglesTable->setColumnCount(1);
    for (int i = 0; i < tiltAngles.size(); ++i)
    {
      QTableWidgetItem* item = new QTableWidgetItem();
      item->setData(Qt::DisplayRole, QString::number(tiltAngles[i]));
      ui.TiltAnglesTable->setItem(i, 0, item);
    }
  }
  else
  {
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
  QTableWidgetItem* item = this->Internals->Ui.TiltAnglesTable->item(row,
                                                                     column);
  QString str = item->data(Qt::DisplayRole).toString();
  if (dsource->type() == DataSource::TiltSeries)
  {
    QVector<double> tiltAngles = dsource->getTiltAngles();
    bool ok;
    double value = str.toDouble(&ok);
    if (ok)
    {
      tiltAngles[row] = value;
      dsource->setTiltAngles(tiltAngles);
    }
    else
    {
      std::cerr << "Invalid tilt angle: " << str.toStdString() << std::endl;
    }
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
}

}
