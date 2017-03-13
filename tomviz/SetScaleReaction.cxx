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
#include "SetScaleReaction.h"

#include "ActiveObjects.h"

#include <vtkImageData.h>
#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>
#include <vtkVector.h>

#include <pqCoreUtilities.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace tomviz {

SetScaleReaction::SetScaleReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void SetScaleReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

void SetScaleReaction::setScale()
{
  // Get the extents, use that to set starting size.
  auto source = ActiveObjects::instance().activeDataSource();
  Q_ASSERT(source);
  auto t =
    vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
  Q_ASSERT(t);
  auto data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  Q_ASSERT(data);
  int* extents = data->GetExtent();
  vtkVector3i extent(extents[1] - extents[0] + 1, extents[3] - extents[2] + 1,
                     extents[5] - extents[4] + 1);
  double* spacing = data->GetSpacing();
  vtkVector3d length((extent[0] - 1) * spacing[0] * 1e9,
                     (extent[1] - 1) * spacing[1] * 1e9,
                     (extent[2] - 1) * spacing[2] * 1e9);

  QDialog dialog(pqCoreUtilities::mainWidget());
  auto layout = new QHBoxLayout;
  auto label = new QLabel("Set volume  dimensions (nm):");
  layout->addWidget(label);
  auto linex = new QLineEdit(QString::number(length[0]));
  auto liney = new QLineEdit(QString::number(length[1]));
  auto linez = new QLineEdit(QString::number(length[2]));

  layout->addWidget(linex);
  layout->addWidget(liney);
  layout->addWidget(linez);
  auto v = new QVBoxLayout;
  auto buttons =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
  v->addLayout(layout);
  v->addWidget(buttons);
  dialog.setLayout(v);

  if (dialog.exec() == QDialog::Accepted) {
    vtkVector3d newLength(linex->text().toDouble() * 1e-9,
                          liney->text().toDouble() * 1e-9,
                          linez->text().toDouble() * 1e-9);
    vtkVector3d newSpacing(newLength[0] / (extent[0] - 1),
                           newLength[1] / (extent[1] - 1),
                           newLength[2] / (extent[2] - 1));
    data->SetSpacing(newSpacing.GetData());
    data->SetOrigin(0, 0, 0);
    spacing = data->GetSpacing();
    source->dataModified();
  }
}
}
