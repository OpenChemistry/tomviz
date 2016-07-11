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
#include "ProgressDialogManager.h"

#include "DataSource.h"
#include "ModuleManager.h"
#include "Operator.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QMainWindow>
#include <QMap>
#include <QProgressBar>
#include <QVBoxLayout>

#include <cassert>
#include <iostream>

namespace tomviz
{

class ProgressDialogManager::PDMInternal
{
public:
  QMap<Operator*, DataSource*> opToDataSource;
  Operator *currentOp;
};

ProgressDialogManager::ProgressDialogManager(QMainWindow *mw)
  : Superclass(mw), mainWindow(mw), Internals(new PDMInternal)
{
  ModuleManager& mm = ModuleManager::instance();
  QObject::connect(&mm, SIGNAL(dataSourceAdded(DataSource*)), this, SLOT(dataSourceAdded(DataSource*)));
}

ProgressDialogManager::~ProgressDialogManager()
{
  delete this->Internals;
}

void ProgressDialogManager::operationStarted()
{
  QDialog *progressDialog = new QDialog(this->mainWindow);
  progressDialog->setAttribute(Qt::WA_DeleteOnClose);

  Operator *op = qobject_cast<Operator*>(this->sender());
  QObject::connect(op, &Operator::transformingDone,
                   progressDialog, &QDialog::accept);
  this->Internals->currentOp = op;
  QObject::connect(op, &Operator::transformingDone, this, &ProgressDialogManager::operationDone);
  QObject::connect(progressDialog, &QDialog::rejected, this, &ProgressDialogManager::operationCanceled);

  QLayout* layout = new QVBoxLayout();
  QWidget* progressWidget = op->getCustomProgressWidget(progressDialog);
  if (progressWidget == nullptr)
  {
    QProgressBar* progressBar = new QProgressBar(progressDialog);
    progressBar->setMinimum(0);
    progressBar->setMaximum(op->totalProgressSteps());
    progressWidget = progressBar;
    QObject::connect(op, &Operator::updateProgress, progressBar, &QProgressBar::valueChanged);
    QObject::connect(op, &Operator::updateProgress, this, &ProgressDialogManager::operationProgress);
    layout->addWidget(progressBar);
  }
  layout->addWidget(progressWidget);
  if (op->supportsCancelingMidTransform())
  {
    // Unless the widget has custom progress handling, you can't cancel it.
    QDialogButtonBox* dialogButtons = new QDialogButtonBox(
        QDialogButtonBox::Cancel, Qt::Horizontal, progressDialog);
    layout->addWidget(dialogButtons);
    QObject::connect(dialogButtons, &QDialogButtonBox::rejected, op, &Operator::cancelTransform);
    QObject::connect(dialogButtons, &QDialogButtonBox::rejected, progressDialog, &QDialog::reject);
  }
  progressDialog->setWindowTitle(QString("%1 Progress").arg(op->label()));
  progressDialog->setLayout(layout);
  progressDialog->adjustSize();
  progressDialog->show();
  QCoreApplication::processEvents();
}

void ProgressDialogManager::operatorAdded(Operator *op)
{
  DataSource *source = qobject_cast<DataSource*>(this->sender());
  QObject::connect(op, &Operator::transformingStarted,
                   this, &ProgressDialogManager::operationStarted);
  this->Internals->opToDataSource.insert(op, source);
}

void ProgressDialogManager::dataSourceAdded(DataSource *ds)
{
  QObject::connect(ds, SIGNAL(operatorAdded(Operator*)),
                   this, SLOT(operatorAdded(Operator*)));
  auto listOfOps = ds->operators();
  for (auto itr = listOfOps.begin(); itr != listOfOps.end(); ++itr)
  {
    this->Internals->opToDataSource.insert(*itr, ds);
  }
}

void ProgressDialogManager::operationProgress(int)
{
  // Probably not strictly neccessary in the long run where we have a background thread.
  // But until then we need this call.
  QCoreApplication::processEvents();
}

void ProgressDialogManager::operationCanceled()
{
}

void ProgressDialogManager::operationDone(bool status)
{
  Operator *op = qobject_cast<Operator*>(this->sender());
  // make sure we are in a sane state, two operators running at once is not handled right now
  assert(op == this->Internals->currentOp);
  // assume cancelled for now, need more info to determine that though.
  if (!status)
  {
    DataSource *ds = this->Internals->opToDataSource.value(op);
    auto listOfOps = ds->operators();
    for (auto itr = listOfOps.begin(); itr != listOfOps.end(); ++itr)
    {
      // If we've found the shared pointer for this one...
      if (op == *itr)
      {
        ds->removeOperator(*itr);
        this->Internals->opToDataSource.erase(this->Internals->opToDataSource.find(op));
        break;
      }
    }
  }
}
}
