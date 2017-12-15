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
#include "AddResampleReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "Utilities.h"
#include <pqCoreUtilities.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkNew.h>
#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>

#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace tomviz {

AddResampleReaction::AddResampleReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void AddResampleReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

namespace {
vtkImageData* imageData(DataSource* source)
{
  auto t = source->producer();
  return vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
}
}

void AddResampleReaction::resample(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source) {
    qDebug() << "Exiting early - no data :-(";
    return;
  }
  vtkImageData* originalData = imageData(source);
  int extents[6];
  originalData->GetExtent(extents);
  int resolution[3] = { extents[1] - extents[0] + 1,
                        extents[3] - extents[2] + 1,
                        extents[5] - extents[4] + 1 };

  // Find out how big they want to resample it
  QDialog dialog(pqCoreUtilities::mainWidget());
  QHBoxLayout* layout = new QHBoxLayout;
  QLabel* label0 = new QLabel(QString("Current resolution: %1, %2, %3")
                                .arg(resolution[0])
                                .arg(resolution[1])
                                .arg(resolution[2]));
  QLabel* label1 = new QLabel("New resolution:");
  layout->addWidget(label1);
  QSpinBox* spinx = new QSpinBox;
  spinx->setRange(2, resolution[0]);
  spinx->setValue(resolution[0]);
  QSpinBox* spiny = new QSpinBox;
  spiny->setRange(2, resolution[1]);
  spiny->setValue(resolution[1]);
  QSpinBox* spinz = new QSpinBox;
  spinz->setRange(2, resolution[2]);
  spinz->setValue(resolution[2]);
  layout->addWidget(spinx);
  layout->addWidget(spiny);
  layout->addWidget(spinz);
  QVBoxLayout* v = new QVBoxLayout;
  QDialogButtonBox* buttons =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
  v->addWidget(label0);
  v->addLayout(layout);
  v->addWidget(buttons);
  dialog.setLayout(v);
  if (dialog.exec() == QDialog::Accepted) {
    // Compute the resampled data
    double origin[3];
    double spacing[3];
    originalData->GetOrigin(origin);
    originalData->GetSpacing(spacing);
    int newResolution[3] = { spinx->value(), spiny->value(), spinz->value() };
    double newOrigin[3], newSpacing[3];
    int newExtents[6];
    for (int i = 0; i < 3; ++i) {
      newOrigin[i] = origin[i] + extents[2 * i] * spacing[i];
      newExtents[2 * i] = 0;
      newExtents[2 * i + 1] = newResolution[i] - 1;
      newSpacing[i] = spacing[i] * (extents[2 * i + 1] - extents[2 * i]) /
                      (double)(newResolution[i]);
    }
    vtkNew<vtkImageReslice> reslice;
    reslice->SetInputData(originalData);
    reslice->SetInterpolationModeToLinear(); // for now
    reslice->SetOutputExtent(newExtents);
    reslice->SetOutputSpacing(newSpacing);
    reslice->SetOutputOrigin(newOrigin);
    reslice->Update();

    // Create a DataSource and set its data to the resampled data
    // TODO - cloning here is really expensive memory-wise, we should figure
    // out a different way to do it
    DataSource* resampledData = source->clone(true);
    QString name =
      resampledData->proxy()->GetAnnotation(Attributes::LABEL);
    name = "Downsampled_" + name;
    resampledData->proxy()->SetAnnotation(Attributes::LABEL,
                                                    name.toLatin1().data());
    auto t = resampledData->producer();
    t->SetOutput(reslice->GetOutput());
    resampledData->dataModified();

    // Add the new DataSource
    LoadDataReaction::dataSourceAdded(resampledData);
  }
}
}
