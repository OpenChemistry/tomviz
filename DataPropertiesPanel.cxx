/******************************************************************************

  This source file is part of the TEM tomography project.

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
#include "vtkDataSetAttributes.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkSMSourceProxy.h"

#include <QPointer>

namespace TEM
{

class DataPropertiesPanel::DPPInternals
{
public:
  Ui::DataPropertiesPanel Ui;
  QPointer<DataSource> CurrentDataSource;

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

    separator = pqProxyWidget::newGroupLabelWidget("Original Data Range", parent);
    l->insertWidget(l->indexOf(ui.OriginalDataRange), separator);

    separator = pqProxyWidget::newGroupLabelWidget("Transformed Data Range", parent);
    l->insertWidget(l->indexOf(ui.TransformedDataRange), separator);

    this->clear();
    }

  void clear()
    {
    Ui::DataPropertiesPanel& ui = this->Ui;
    ui.FileName->setText("");
    ui.Dimensions->setText("");
    ui.OriginalDataRange->setText("");
    ui.TransformedDataRange->setText("");
    }

};

//-----------------------------------------------------------------------------
DataPropertiesPanel::DataPropertiesPanel(QWidget* parentObject)
  : Superclass(parentObject),
  Internals(new DataPropertiesPanel::DPPInternals(this))
{
  this->connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
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
    this->connect(dsource, SIGNAL(dataChanged()), SLOT(update()), Qt::UniqueConnection);
    }
  this->update();
}

//-----------------------------------------------------------------------------
void DataPropertiesPanel::update()
{
  DataSource* dsource = this->Internals->CurrentDataSource;
  if (!dsource)
    {
    this->Internals->clear();
    return;
    }

  Ui::DataPropertiesPanel& ui = this->Internals->Ui;
  ui.FileName->setText(dsource->filename());

  vtkPVDataInformation* odInfo = dsource->originalDataSource()->GetDataInformation(0);
  vtkPVDataInformation* tdInfo = dsource->producer()->GetDataInformation(0);

  ui.Dimensions->setText(
    QString("%1 x %2 x %3")
    .arg(odInfo->GetExtent()[1] - odInfo->GetExtent()[0] + 1)
    .arg(odInfo->GetExtent()[3] - odInfo->GetExtent()[2] + 1)
    .arg(odInfo->GetExtent()[5] - odInfo->GetExtent()[4] + 1));

  vtkPVArrayInformation* oscalars = odInfo->GetPointDataInformation()->GetAttributeInformation(
    vtkDataSetAttributes::SCALARS);
  ui.OriginalDataRange->setText(
    QString("%1 : %2")
    .arg(oscalars->GetComponentRange(0)[0])
    .arg(oscalars->GetComponentRange(0)[1]));

  vtkPVArrayInformation* tscalars = tdInfo->GetPointDataInformation()->GetAttributeInformation(
    vtkDataSetAttributes::SCALARS);

  ui.TransformedDataRange->setText(
    QString("%1 : %2")
    .arg(tscalars->GetComponentRange(0)[0])
    .arg(tscalars->GetComponentRange(0)[1]));
}

//-----------------------------------------------------------------------------
}
