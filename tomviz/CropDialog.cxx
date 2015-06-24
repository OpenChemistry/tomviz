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
#include "CropDialog.h"
#include "ui_CropDialog.h"

#include <QPoint>
#include <QVariant>

#include <vtkEventQtSlotConnect.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkMath.h>
#include <vtkObject.h>
#include <vtkTrivialProducer.h>

#include <pqSettings.h>
#include <vtkSMSourceProxy.h>
#include <pqApplicationCore.h>
#include <vtkBoundingBox.h>

#include <algorithm>
#include <math.h>

#include "DataSource.h"


namespace tomviz
{

class CropDialog::CDInternals
{
public:
  Ui::CropDialog ui;
  DataSource* dataSource;
  int* dataExtent;
  vtkBoundingBox dataBoundingBox;

  void bounds(int bs[6]) const
    {
    int index = 0;
    bs[index++] = this->ui.startX->value();
    bs[index++] = this->ui.endX->value();
    bs[index++] = this->ui.startY->value();
    bs[index++] = this->ui.endY->value();
    bs[index++] = this->ui.startZ->value();
    bs[index++] = this->ui.endZ->value();
    }

  void savePosition(const QPoint& pos)
    {
    QSettings* settings = pqApplicationCore::instance()->settings();
    settings->setValue("cropDialogPosition", QVariant(pos));
    }

  QVariant loadPosition()
    {
    QSettings* settings = pqApplicationCore::instance()->settings();

    return settings->value("cropDialogPosition");
    }

  void blockSpinnerSignals(bool block)
    {
    ui.startX->blockSignals(block);
    ui.startY->blockSignals(block);
    ui.startZ->blockSignals(block);
    ui.endX->blockSignals(block);
    ui.endY->blockSignals(block);
    ui.endZ->blockSignals(block);
    }
};

//-----------------------------------------------------------------------------
CropDialog::CropDialog(QWidget* parentObject, DataSource* source)
  : Superclass(parentObject),
  Internals (new CropDialog::CDInternals())
{
  this->Internals->dataSource = source;

  Ui::CropDialog& ui = this->Internals->ui;
  ui.setupUi(this);

  QVariant position = this->Internals->loadPosition();
  if (!position.isNull())
    {
      this->move(position.toPoint());
    }

  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());

  vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  int *extent = imageData->GetExtent();
  this->Internals->dataExtent = extent;

  double e[6];
  std::copy(extent, extent+6, e);
  this->Internals->dataBoundingBox.SetBounds(e);

  // Set ranges and default values
  ui.startX->setRange(extent[0], extent[1]);
  ui.startX->setValue(extent[0]);
  ui.startY->setRange(extent[2], extent[3]);
  ui.startY->setValue(extent[2]);
  ui.startZ->setRange(extent[4], extent[5]);
  ui.startZ->setValue(extent[4]);

  ui.endX->setRange(extent[0], extent[1]);
  ui.endX->setValue(extent[1]);
  ui.endY->setRange(extent[2], extent[3]);
  ui.endY->setValue(extent[3]);
  ui.endZ->setRange(extent[4], extent[5]);
  ui.endZ->setValue(extent[5]);

  this->connect(this, SIGNAL(accepted()), SLOT(crop()));
  this->connect(this, SIGNAL(rejected()), SLOT(cancel()));

  this->connect(ui.startX, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.startY, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.startZ, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.endX, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.endY, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
  this->connect(ui.endZ, SIGNAL(valueChanged(int)),
                this, SLOT(valueChanged()));
}

//-----------------------------------------------------------------------------
CropDialog::~CropDialog()
{
}

//-----------------------------------------------------------------------------
void CropDialog::crop()
{
  int cropVolume[6];

  this->Internals->bounds(cropVolume);
  this->Internals->dataSource->crop(cropVolume);
  this->Internals->savePosition(this->pos());
}

//-----------------------------------------------------------------------------
void CropDialog::cancel()
{
  this->Internals->savePosition(this->pos());
}

//-----------------------------------------------------------------------------
void CropDialog::updateBounds(double *newBounds)
{
  Ui::CropDialog& ui = this->Internals->ui;

  this->Internals->blockSpinnerSignals(true);

  vtkBoundingBox newBoundingBox(newBounds);

  if (this->Internals->dataBoundingBox.Intersects(newBoundingBox))
    {
    ui.startX->setValue(vtkMath::Round(newBounds[0]));
    ui.startY->setValue(vtkMath::Round(newBounds[2]));
    ui.startZ->setValue(vtkMath::Round(newBounds[4]));

    ui.endX->setValue(vtkMath::Round(newBounds[1]));
    ui.endY->setValue(vtkMath::Round(newBounds[3]));
    ui.endZ->setValue(vtkMath::Round(newBounds[5]));
    }
  // If there is no intersection use data extent
  else
    {
    ui.startX->setValue(this->Internals->dataExtent[0]);
    ui.startY->setValue(this->Internals->dataExtent[2]);
    ui.startZ->setValue(this->Internals->dataExtent[4]);

    ui.endX->setValue(this->Internals->dataExtent[1]);
    ui.endY->setValue(this->Internals->dataExtent[3]);
    ui.endZ->setValue(this->Internals->dataExtent[5]);

    }

  this->Internals->blockSpinnerSignals(false);
}

//-----------------------------------------------------------------------------
void CropDialog::valueChanged()
{
  int cropVolume[6];

  this->Internals->bounds(cropVolume);

  emit this->bounds(cropVolume);
}

}
