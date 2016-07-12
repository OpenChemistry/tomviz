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

#include "SetTiltAnglesReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

#include <QDialog>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QSpinBox>
#include <QDialogButtonBox>

namespace tomviz
{

SetTiltAnglesReaction::SetTiltAnglesReaction(QAction *p, QMainWindow *mw)
  : Superclass(p), mainWindow(mw)
{
  this->connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateEnableState()));
  updateEnableState();
}

SetTiltAnglesReaction::~SetTiltAnglesReaction()
{
}

void SetTiltAnglesReaction::updateEnableState()
{
  bool enable = ActiveObjects::instance().activeDataSource() != nullptr;
  if (enable)
  {
    enable = ActiveObjects::instance().activeDataSource()->type() == DataSource::TiltSeries;
  }
  parentAction()->setEnabled(enable);
}

void SetTiltAnglesReaction::showSetTiltAnglesUI(QMainWindow *window, DataSource *source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
  {
    return;
  }
  QDialog dialog(window);
  QGridLayout *layout = new QGridLayout;

  QString description_string =
    "A tomographic \"tilt series\" is a set of projection images taken while rotating"
    " (\"tilting\") the specimen.  Setting the correct angles is needed for accurate"
    " reconstruction.\n  Set a linearly spaced range of angles by specifying the start"
    " and end tilt index and start and end angles.  Note, tilt angles can also be set"
    " in the \"Data Properties\" panel or within Python.";

  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    source->producer()->GetClientSideObject());
  vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  int extent[6];
  data->GetExtent(extent);
  int totalSlices = extent[5] - extent[4] + 1;
  double angleIncrement = 1.0;
  if (totalSlices < 60)
  {
    angleIncrement = 3.0;
  }
  else if (totalSlices < 80)
  {
    angleIncrement = 2.0;
  }
  else if (totalSlices < 120)
  {
    angleIncrement = 1.5;
  }

  QLabel *descriptionLabel = new QLabel(description_string);
  descriptionLabel->setWordWrap(true);
  layout->addWidget(descriptionLabel, 0, 0, 1, 4, Qt::AlignCenter);
  layout->addWidget(new QLabel("Start Image #: "), 1, 0, 1, 1, Qt::AlignCenter);
  QSpinBox *startTilt = new QSpinBox(&dialog);
  startTilt->setRange(0,totalSlices-1);
  startTilt->setValue(0);
  layout->addWidget(startTilt, 1, 1, 1, 1, Qt::AlignCenter);
  layout->addWidget(new QLabel("End Image #: "), 2, 0, 1, 1, Qt::AlignCenter);
  QSpinBox *endTilt = new QSpinBox(&dialog);
  endTilt->setRange(0,totalSlices-1);
  endTilt->setValue(totalSlices-1);
  layout->addWidget(endTilt, 2, 1, 1, 1, Qt::AlignCenter);
  layout->addWidget(new QLabel("Set Start Angle: "), 1, 2, 1, 1, Qt::AlignCenter);
  QDoubleSpinBox *startAngle = new QDoubleSpinBox(&dialog);
  startAngle->setRange(-360.0, 360.0);
  startAngle->setValue(-(totalSlices - 1) * angleIncrement / 2.0);
  layout->addWidget(startAngle, 1, 3, 1, 1, Qt::AlignCenter);
  layout->addWidget(new QLabel("Set End Angle: "), 2, 2, 1, 1, Qt::AlignCenter);
  QDoubleSpinBox *endAngle = new QDoubleSpinBox(&dialog);
  endAngle->setRange(-360.0, 360.0);
  endAngle->setValue(startAngle->value() + (totalSlices - 1) * angleIncrement);
  layout->addWidget(endAngle, 2, 3, 1, 1, Qt::AlignCenter);
  
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                   | QDialogButtonBox::Cancel);
  connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

  layout->addWidget(buttons, 3, 0, 1, 4, Qt::AlignCenter);
  dialog.setLayout(layout);
  dialog.setWindowTitle("Set Tilt Angles");

  if (dialog.exec() == QDialog::Accepted)
  {
    QVector<double> tiltAngles = source->getTiltAngles();
    int start = startTilt->value();
    int end = endTilt->value();
    if (end == start)
    {
      tiltAngles[start] = startAngle->value();
    }
    else
    {
      double delta = (endAngle->value() - startAngle->value()) / (end - start);
      double baseAngle = startAngle->value();
      if (delta < 0)
      {
        int temp = start;
        start = end;
        end = temp;
        baseAngle = endAngle->value();
      }
      for (int i = 0; start + i <= end; ++i)
      {
        tiltAngles[start + i] =  baseAngle + delta * i;
      }
    }
    source->setTiltAngles(tiltAngles);
  }
}

}
