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
#include "AddPythonTransformReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "OperatorPython.h"
#include "pqCoreUtilities.h"
#include "EditPythonOperatorDialog.h"

#include <vtkImageData.h>
#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>

#include <QDialog>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

namespace tomviz
{
//-----------------------------------------------------------------------------
AddPythonTransformReaction::AddPythonTransformReaction(QAction* parentObject,
                                               const QString &l,
                                               const QString &s)
  : Superclass(parentObject), scriptLabel(l), scriptSource(s),
    interactive(false)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

//-----------------------------------------------------------------------------
AddPythonTransformReaction::~AddPythonTransformReaction()
{
}

//-----------------------------------------------------------------------------
void AddPythonTransformReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != NULL &&
        ActiveObjects::instance().activeDataSource()->type() == DataSource::Volume);
}

//-----------------------------------------------------------------------------
OperatorPython* AddPythonTransformReaction::addExpression(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
    {
    return NULL;
    }

  OperatorPython *opPython = new OperatorPython();
  QSharedPointer<Operator> op(opPython);
  opPython->setLabel(scriptLabel);
  opPython->setScript(scriptSource);

  // Shift uniformly, crop, both have custom gui
  if (scriptLabel == "Shift Uniformly")
    {
    vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int *extent = data->GetExtent();

    QDialog dialog(pqCoreUtilities::mainWidget());
    QHBoxLayout *layout = new QHBoxLayout;
    QLabel *label = new QLabel("Shift to apply:");
    layout->addWidget(label);
    QSpinBox *spinx = new QSpinBox;
    spinx->setRange(extent[0], extent[1]);
    spinx->setValue(extent[0]);
    QSpinBox *spiny = new QSpinBox;
    spiny->setRange(extent[2], extent[3]);
    spiny->setValue(extent[2]);
    QSpinBox *spinz = new QSpinBox;
    spinz->setRange(extent[4], extent[5]);
    spinz->setValue(extent[4]);
    layout->addWidget(spinx);
    layout->addWidget(spiny);
    layout->addWidget(spinz);
    QVBoxLayout *v = new QVBoxLayout;
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                     | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    v->addLayout(layout);
    v->addWidget(buttons);
    dialog.setLayout(v);

    if (dialog.exec() == QDialog::Accepted)
      {
      QString shiftScript = scriptSource;
      shiftScript.replace("###SHIFT###",
                          QString("SHIFT = [%1, %2, %3]").arg(spinx->value())
                          .arg(spiny->value()).arg(spinz->value()));
      opPython->setScript(shiftScript);
      source->addOperator(op);
      }
    }
  else if (scriptLabel == "Crop")
    {
    vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int *extent = data->GetExtent();

    QDialog dialog(pqCoreUtilities::mainWidget());
    QHBoxLayout *layout1 = new QHBoxLayout;
    QLabel *label = new QLabel("Crop data start:");
    layout1->addWidget(label);
    QSpinBox *spinx = new QSpinBox;
    spinx->setRange(extent[0], extent[1]);
    spinx->setValue(extent[0]);
    QSpinBox *spiny = new QSpinBox;
    spiny->setRange(extent[2], extent[3]);
    spiny->setValue(extent[2]);
    QSpinBox *spinz = new QSpinBox;
    spinz->setRange(extent[4], extent[5]);
    spinz->setValue(extent[4]);
    layout1->addWidget(label);
    layout1->addWidget(spinx);
    layout1->addWidget(spiny);
    layout1->addWidget(spinz);
    QHBoxLayout *layout2 = new QHBoxLayout;
    label = new QLabel("Crop data end:");
    layout2->addWidget(label);
    QSpinBox *spinxx = new QSpinBox;
    spinxx->setRange(extent[0], extent[1]);
    spinxx->setValue(extent[1]);
    QSpinBox *spinyy = new QSpinBox;
    spinyy->setRange(extent[2], extent[3]);
    spinyy->setValue(extent[3]);
    QSpinBox *spinzz = new QSpinBox;
    spinzz->setRange(extent[4], extent[5]);
    spinzz->setValue(extent[5]);
    layout2->addWidget(label);
    layout2->addWidget(spinxx);
    layout2->addWidget(spinyy);
    layout2->addWidget(spinzz);
    QVBoxLayout *v = new QVBoxLayout;
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                     | QDialogButtonBox::Cancel);
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    v->addLayout(layout1);
    v->addLayout(layout2);
    v->addWidget(buttons);
    dialog.setLayout(v);

    if (dialog.exec() == QDialog::Accepted)
      {
      QString cropScript = scriptSource;
      cropScript.replace("###START_CROP###",
                          QString("START_CROP = [%1, %2, %3]").arg(spinx->value())
                          .arg(spiny->value()).arg(spinz->value()));
      cropScript.replace("###END_CROP###",
                          QString("END_CROP = [%1, %2, %3]").arg(spinxx->value())
                          .arg(spinyy->value()).arg(spinzz->value()));
      opPython->setScript(cropScript);
      source->addOperator(op);
      }
    }
  else if (scriptLabel == "Rotate")
  {
      QDialog dialog(pqCoreUtilities::mainWidget());
      QHBoxLayout *layout1 = new QHBoxLayout;
      QLabel *label = new QLabel("Rotate Angle:");
      layout1->addWidget(label);
      QDoubleSpinBox *angle = new QDoubleSpinBox;
      angle->setRange(0, 360);
      angle->setValue(0);
      layout1->addWidget(label);
      layout1->addWidget(angle);
      QHBoxLayout *layout2 = new QHBoxLayout;
      label = new QLabel("Rotate Axis:");
      layout2->addWidget(label);
      QSpinBox *axis = new QSpinBox;
      axis->setRange(0, 2);
      axis->setValue(0);
      layout2->addWidget(label);
      layout2->addWidget(axis);
      QVBoxLayout *v = new QVBoxLayout;
      QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                       | QDialogButtonBox::Cancel);
      connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
      connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
      v->addLayout(layout1);
      v->addLayout(layout2);
      v->addWidget(buttons);
      dialog.setLayout(v);
      
      if (dialog.exec() == QDialog::Accepted)
      {
          QString cropScript = scriptSource;
          cropScript.replace("###ROT_AXIS###",
                             QString("ROT_AXIS = %1").arg(axis->value()) );
          cropScript.replace("###ROT_ANGLE###",
                             QString("ROT_ANGLE = %1").arg(angle->value()) );
          opPython->setScript(cropScript);
          source->addOperator(op);
      }
  }
  else if (interactive)
    {
    // Create a non-modal dialog, delete it once it has been closed.
    EditPythonOperatorDialog *dialog =
        new EditPythonOperatorDialog(op, pqCoreUtilities::mainWidget());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(dialog, SIGNAL(accepted()), SLOT(addOperator()));
    dialog->show();
    }
  else
    {
    source->addOperator(op);
    }
  return NULL;
}

void AddPythonTransformReaction::addOperator()
{
  EditPythonOperatorDialog *dialog =
      qobject_cast<EditPythonOperatorDialog*>(sender());
  if (!dialog)
    return;
  DataSource *source = ActiveObjects::instance().activeDataSource();
  if (!source)
    {
    return;
    }
  source->addOperator(dialog->op());
}

}
