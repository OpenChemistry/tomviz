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
#include "pqPropertiesPanel.h"
#include "pqProxyWidget.h"
#include "Utilities.h"
#include "vtkDataSetAttributes.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "pqView.h"

#include "vtkSMSourceProxy.h"
#include "vtkDataObject.h"
#include "vtkFieldData.h"
#include "vtkDataArray.h"
#include "vtkAlgorithm.h"

#include <QPointer>

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

    separator = pqProxyWidget::newGroupLabelWidget("Dimensions", parent);
    l->insertWidget(l->indexOf(ui.Dimensions), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Original Data Range",
                                                   parent);
    l->insertWidget(l->indexOf(ui.OriginalDataRange), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Transformed Data Range",
                                                   parent);
    l->insertWidget(l->indexOf(ui.TransformedDataRange), separator);

    this->TiltAnglesSeparator =
      pqProxyWidget::newGroupLabelWidget("Tilt Angles", parent);
    l->insertWidget(l->indexOf(ui.TiltAnglesTable), this->TiltAnglesSeparator);

    // set icons for save/restore buttons.
    ui.ColorMapSaveAsDefaults->setIcon(
      ui.ColorMapSaveAsDefaults->style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui.ColorMapRestoreDefaults->setIcon(
      ui.ColorMapRestoreDefaults->style()->standardIcon(QStyle::SP_BrowserReload));

    this->clear();
    }

  void clear()
    {
    Ui::DataPropertiesPanel& ui = this->Ui;
    ui.FileName->setText("");
    ui.Dimensions->setText("");
    ui.OriginalDataRange->setText("");
    ui.TransformedDataRange->setText("");
    if (this->ColorMapWidget)
      {
      ui.verticalLayout->removeWidget(this->ColorMapWidget);
      delete this->ColorMapWidget;
      }
    this->TiltAnglesSeparator->hide();
    ui.TiltAnglesTable->clear();
    ui.TiltAnglesTable->setRowCount(0);
    ui.TiltAnglesTable->hide();
    }

};

//-----------------------------------------------------------------------------
DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : Superclass(parentObject),
  Internals(new DataPropertiesPanel::DPPInternals(this))
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(setDataSource(DataSource*)));
}

//-----------------------------------------------------------------------------
DataPropertiesPanel::~DataPropertiesPanel()
{
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::setDataSource(DataSource* dsource)
{
  if (this->Internals->CurrentDataSource)
    {
    this->disconnect(this->Internals->CurrentDataSource);
    }
  this->Internals->CurrentDataSource = dsource;
  if (dsource)
    {
    this->connect(dsource, SIGNAL(dataChanged()), SLOT(update()),
                  Qt::UniqueConnection);
    }
  this->update();
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::update()
{
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

  vtkPVDataInformation* tdInfo = dsource->producer()->GetDataInformation(0);

  ui.Dimensions->setText(QString("%1 x %2 x %3")
                         .arg(tdInfo->GetExtent()[1] - tdInfo->GetExtent()[0] + 1)
                         .arg(tdInfo->GetExtent()[3] - tdInfo->GetExtent()[2] + 1)
                         .arg(tdInfo->GetExtent()[5] - tdInfo->GetExtent()[4] + 1));

  if (vtkPVArrayInformation* oscalars = tomviz::scalarArrayInformation(
      dsource->originalDataSource()))
    {
    ui.OriginalDataRange->setText(QString("%1 : %2")
                                  .arg(oscalars->GetComponentRange(0)[0])
                                  .arg(oscalars->GetComponentRange(0)[1]));
    }

  if (vtkPVArrayInformation* tscalars = tomviz::scalarArrayInformation(
      dsource->producer()))
    {
    ui.TransformedDataRange->setText(QString("%1 : %2")
                                     .arg(tscalars->GetComponentRange(0)[0])
                                     .arg(tscalars->GetComponentRange(0)[1]));
    }

  pqProxyWidget* colorMapWidget = new pqProxyWidget(dsource->colorMap());
  colorMapWidget->setApplyChangesImmediately(true);
  colorMapWidget->updatePanel();
  ui.verticalLayout->insertWidget(ui.verticalLayout->indexOf(ui.TiltAnglesTable)-1,
                                  colorMapWidget);
  colorMapWidget->connect(ui.ColorMapExpander, SIGNAL(toggled(bool)),
                          SLOT(setVisible(bool)));
  colorMapWidget->setVisible(ui.ColorMapExpander->checked());
  colorMapWidget->connect(ui.ColorMapSaveAsDefaults, SIGNAL(clicked()),
                          SLOT(saveAsDefaults()));
  colorMapWidget->connect(ui.ColorMapRestoreDefaults, SIGNAL(clicked()),
                          SLOT(restoreDefaults()));
  this->connect(colorMapWidget, SIGNAL(changeFinished()), SLOT(render()));
  this->Internals->ColorMapWidget = colorMapWidget;

  // display tilt series data
  if (dsource->type() == DataSource::TiltSeries)
    {
    this->Internals->TiltAnglesSeparator->show();
    ui.TiltAnglesTable->show();
    vtkDataArray* tiltAngles = vtkAlgorithm::SafeDownCast(
        dsource->producer()->GetClientSideObject())
      ->GetOutputDataObject(0)->GetFieldData()->GetArray("tilt_angles");
    ui.TiltAnglesTable->setRowCount(tiltAngles->GetNumberOfTuples());
    ui.TiltAnglesTable->setColumnCount(tiltAngles->GetNumberOfComponents());
    for (int i = 0; i < tiltAngles->GetNumberOfTuples(); ++i)
      {
      double* angles = tiltAngles->GetTuple(i);
      for (int j = 0; j < tiltAngles->GetNumberOfComponents(); ++j)
        {
        QTableWidgetItem* item = new QTableWidgetItem();
        item->setData(Qt::DisplayRole, QString("%1").arg(angles[j]));
        ui.TiltAnglesTable->setItem(i, j, item);
        }
      }
    }
  else
    {
    this->Internals->TiltAnglesSeparator->hide();
    ui.TiltAnglesTable->hide();
    }
  this->connect(this->Internals->Ui.TiltAnglesTable,
      SIGNAL(cellChanged(int, int)),
      SLOT(onTiltAnglesModified(int, int)));
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::render()
{
  pqView* view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view)
    {
    view->render();
    }
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::onTiltAnglesModified(int row, int column)
{
  DataSource* dsource = this->Internals->CurrentDataSource;
  QTableWidgetItem* item = this->Internals->Ui.TiltAnglesTable->item(row, column);
  QString str = item->data(Qt::DisplayRole).toString();
  if (dsource->type() == DataSource::TiltSeries)
    {
    vtkDataArray* tiltAngles = vtkAlgorithm::SafeDownCast(
        dsource->producer()->GetClientSideObject())
      ->GetOutputDataObject(0)->GetFieldData()->GetArray("tilt_angles");
    bool ok;
    double value = str.toDouble(&ok);
    if (ok)
      {
      double* tuple = tiltAngles->GetTuple(row);
      tuple[column] = value;
      tiltAngles->SetTuple(row, tuple);
      }
    else
      {
      std::cerr << "Invalid tilt angle: " << str.toStdString() << std::endl;
      }
    }
}

//-----------------------------------------------------------------------------
}
