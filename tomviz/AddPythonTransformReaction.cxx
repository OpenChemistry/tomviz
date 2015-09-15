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
#include "EditOperatorDialog.h"

#include <vtkImageData.h>
#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>

#include <QComboBox>
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
        ActiveObjects::instance().activeDataSource() != NULL);
}

    
//Update minimum value allowed for last slice
void AddPythonTransformReaction::updateLastSliceMin(int min){
    lastSlice->setMinimum(min);
}

//Update maximum value allowed for first and last slices
//a=0,1,2 represents axis
void AddPythonTransformReaction::updateSliceMax(int a){
    firstSlice->setMaximum(shape[2*a+1]-shape[2*a]);
    lastSlice->setMaximum(shape[2*a+1]-shape[2*a]);
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
    QLabel *label = new QLabel("Shift to apply:", &dialog);
    layout->addWidget(label);
    QSpinBox *spinx = new QSpinBox(&dialog);
    spinx->setRange(-(extent[1]-extent[0]), extent[1]-extent[0]);
    spinx->setValue(0);
    QSpinBox *spiny = new QSpinBox(&dialog);
    spiny->setRange(-(extent[3]-extent[2]), extent[3]-extent[2]);
    spiny->setValue(0);
    QSpinBox *spinz = new QSpinBox(&dialog);
    spinz->setRange(-(extent[5]-extent[4]), extent[5]-extent[4]);
    spinz->setValue(0);
    layout->addWidget(spinx);
    layout->addWidget(spiny);
    layout->addWidget(spinz);
    QVBoxLayout *v = new QVBoxLayout;
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                     | QDialogButtonBox::Cancel,
                                                     Qt::Horizontal,
                                                     &dialog);
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
    QLabel *label = new QLabel("Crop data start:", &dialog);
    layout1->addWidget(label);
    QSpinBox *spinx = new QSpinBox(&dialog);
    spinx->setRange(extent[0], extent[1]);
    spinx->setValue(extent[0]);
    QSpinBox *spiny = new QSpinBox(&dialog);
    spiny->setRange(extent[2], extent[3]);
    spiny->setValue(extent[2]);
    QSpinBox *spinz = new QSpinBox(&dialog);
    spinz->setRange(extent[4], extent[5]);
    spinz->setValue(extent[4]);
    layout1->addWidget(label);
    layout1->addWidget(spinx);
    layout1->addWidget(spiny);
    layout1->addWidget(spinz);
    QHBoxLayout *layout2 = new QHBoxLayout;
    label = new QLabel("Crop data end:", &dialog);
    layout2->addWidget(label);
    QSpinBox *spinxx = new QSpinBox(&dialog);
    spinxx->setRange(extent[0], extent[1]);
    spinxx->setValue(extent[1]);
    QSpinBox *spinyy = new QSpinBox(&dialog);
    spinyy->setRange(extent[2], extent[3]);
    spinyy->setValue(extent[3]);
    QSpinBox *spinzz = new QSpinBox(&dialog);
    spinzz->setRange(extent[4], extent[5]);
    spinzz->setValue(extent[5]);
    layout2->addWidget(label);
    layout2->addWidget(spinxx);
    layout2->addWidget(spinyy);
    layout2->addWidget(spinzz);
    QVBoxLayout *v = new QVBoxLayout;
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                     | QDialogButtonBox::Cancel,
                                                     Qt::Horizontal,
                                                     &dialog);
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
      QLabel *label = new QLabel("Rotate Angle:", &dialog);
      layout1->addWidget(label);
      QDoubleSpinBox *angle = new QDoubleSpinBox(&dialog);
      angle->setRange(0, 360);
      angle->setValue(90);
      layout1->addWidget(label);
      layout1->addWidget(angle);
      QHBoxLayout *layout2 = new QHBoxLayout;
      label = new QLabel("Rotate Axis:", &dialog);
      layout2->addWidget(label);
      QComboBox *axis = new QComboBox(&dialog);
      axis->addItem("X");
      axis->addItem("Y");
      axis->addItem("Z");
      axis->setCurrentIndex(2);
      layout2->addWidget(label);
      layout2->addWidget(axis);
      QVBoxLayout *v = new QVBoxLayout;
      QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                       | QDialogButtonBox::Cancel,
                                                       Qt::Horizontal,
                                                       &dialog);
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
                             QString("ROT_AXIS = %1").arg(axis->currentIndex()) );
          cropScript.replace("###ROT_ANGLE###",
                             QString("ROT_ANGLE = %1").arg(angle->value()) );
          opPython->setScript(cropScript);
          source->addOperator(op);
      }
  }
    
  else if (scriptLabel == "Delete Slices")
  {
      vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
                                                               source->producer()->GetClientSideObject());
      vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
      shape = data->GetExtent();

      QDialog dialog(pqCoreUtilities::mainWidget());
      QHBoxLayout *layout1 = new QHBoxLayout;
      QLabel *label = new QLabel("Start:", &dialog);
      layout1->addWidget(label);

      firstSlice = new QSpinBox(&dialog); //starting slice
      firstSlice->setRange(0,shape[5]-shape[4]);
      firstSlice->setValue(0);
      layout1->addWidget(firstSlice);
      
      label = new QLabel("End:", &dialog);
      layout1->addWidget(label);

      lastSlice = new QSpinBox(&dialog); //end slice
      lastSlice->setRange(0,shape[5]-shape[4]);
      lastSlice->setMinimum(0);
      lastSlice->setValue(0);
      //update minimum value allowed in last slice when first slice changes
      connect(firstSlice,SIGNAL(valueChanged(int)),SLOT(updateLastSliceMin(int)));
      layout1->addWidget(lastSlice);
      
      QHBoxLayout *layout2 = new QHBoxLayout;
      label = new QLabel("Axis:", &dialog);
      layout2->addWidget(label);
      QComboBox *axis = new QComboBox(&dialog);
      axis->addItem("X");
      axis->addItem("Y");
      axis->addItem("Z");
      axis->setCurrentIndex(2);
      //update maximum for first and last slices when axis is changed
      connect(axis,SIGNAL(currentIndexChanged(int)),SLOT(updateSliceMax(int)));
      layout2->addWidget(axis);
      
      QVBoxLayout *v = new QVBoxLayout;
      QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                       | QDialogButtonBox::Cancel,
                                                       Qt::Horizontal,
                                                       &dialog);
      connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
      connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
      v->addLayout(layout1);
      v->addLayout(layout2);
      v->addWidget(buttons);
      dialog.setLayout(v);
      dialog.setWindowTitle("Delete Slices");
      dialog.layout()->setSizeConstraint(QLayout::SetFixedSize); //Make the UI non-resizeable
      
      if (dialog.exec() == QDialog::Accepted)
      {
          QString cropScript = scriptSource;
          cropScript.replace("###firstSlice###",
                             QString("firstSlice = %1").arg(firstSlice->value()) );
          cropScript.replace("###lastSlice###",
                             QString("lastSlice = %1").arg(lastSlice->value()) );
          cropScript.replace("###axis###",
                             QString("axis = %1").arg(axis->currentIndex()) );

          opPython->setScript(cropScript);
          source->addOperator(op);
      }
  }

  else if (scriptLabel == "Sobel Filter") //UI for Sobel Filter
  {
      QDialog dialog(pqCoreUtilities::mainWidget());
      QHBoxLayout *layout = new QHBoxLayout;
      QLabel *label = new QLabel("Axis:", &dialog);
      layout->addWidget(label);
      QComboBox *axis = new QComboBox(&dialog);
      axis->addItem("X");
      axis->addItem("Y");
      axis->addItem("Z");
      axis->setCurrentIndex(2);
      layout->addWidget(label);
      layout->addWidget(axis);
      QVBoxLayout *v = new QVBoxLayout;
      QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                                       | QDialogButtonBox::Cancel,
                                                       Qt::Horizontal,
                                                       &dialog);
      connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
      connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
      v->addLayout(layout);
      v->addWidget(buttons);
      dialog.setLayout(v);
      
      if (dialog.exec() == QDialog::Accepted)
      {
          QString cropScript = scriptSource;
          cropScript.replace("###Filter_AXIS###",
                             QString("Filter_AXIS = %1").arg(axis->currentIndex()) );
          opPython->setScript(cropScript);
          source->addOperator(op);
      }
  }
    
  else if (scriptLabel == "Resample")
  {
      vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
                                                               source->producer()->GetClientSideObject());
      vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
      int *extent = data->GetExtent();
      
      QDialog dialog(pqCoreUtilities::mainWidget());
      QGridLayout *layout = new QGridLayout;
      //Add labels
      QLabel *labelx = new QLabel("x:");
      layout->addWidget(labelx,0,1,1,1,Qt::AlignCenter);
      QLabel *labely = new QLabel("y:");
      layout->addWidget(labely,0,2,1,1,Qt::AlignCenter);
      QLabel *labelz = new QLabel("z:");
      layout->addWidget(labelz,0,3,1,1,Qt::AlignCenter);
      QLabel *label = new QLabel("Scale Factors:");
      layout->addWidget(label,1,0,1,1);
      
      QDoubleSpinBox *spinx = new QDoubleSpinBox;
      spinx->setSingleStep(0.5);
      spinx->setValue(1);
      
      QDoubleSpinBox *spiny = new QDoubleSpinBox;
      spiny->setSingleStep(0.5);
      spiny->setValue(1);
      
      QDoubleSpinBox *spinz = new QDoubleSpinBox;
      spinz->setSingleStep(0.5);
      spinz->setValue(1);
      
      layout->addWidget(spinx,1,1,1,1);
      layout->addWidget(spiny,1,2,1,1);
      layout->addWidget(spinz,1,3,1,1);
      
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
          shiftScript.replace("###resampingFactor###",
                              QString("resampingFactor = [%1, %2, %3]").arg(spinx->value())
                              .arg(spiny->value()).arg(spinz->value()));
          opPython->setScript(shiftScript);
          source->addOperator(op);
      }
  }
    
  else if (interactive)
    {
    // Create a non-modal dialog, delete it once it has been closed.
    EditOperatorDialog *dialog =
        new EditOperatorDialog(op, source, pqCoreUtilities::mainWidget());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->show();
    }
  else
    {
    source->addOperator(op);
    }
  return NULL;
}

}
