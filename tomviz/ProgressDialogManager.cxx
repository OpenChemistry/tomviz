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
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QProgressBar>

#include <iostream>

namespace tomviz
{

ProgressDialogManager::ProgressDialogManager(QMainWindow *mw)
  : Superclass(mw), mainWindow(mw)
{
  ModuleManager& mm = ModuleManager::instance();
  QObject::connect(&mm, SIGNAL(dataSourceAdded(DataSource*)), this, SLOT(dataSourceAdded(DataSource*)));
}

ProgressDialogManager::~ProgressDialogManager()
{
}

void ProgressDialogManager::operationStarted()
{
  QDialog *progressDialog = new QDialog(this->mainWindow);
  progressDialog->setAttribute(Qt::WA_DeleteOnClose);

  Operator *op = qobject_cast<Operator*>(this->sender());
  QObject::connect(op, &Operator::transformingDone,
                   progressDialog, &QDialog::accept);

  QLayout* layout = new QHBoxLayout();
  QWidget* progressWidget = op->getCustomProgressWidget(progressDialog);
  if (progressWidget == nullptr)
  {
    QProgressBar* progressBar = new QProgressBar(progressDialog);
    progressBar->setMinimum(0);
    progressBar->setMaximum(op->totalProgressSteps());
    progressWidget = progressBar;
    QObject::connect(op, &Operator::updateProgress, progressBar, &QProgressBar::valueChanged);
    QObject::connect(op, &Operator::updateProgress, this, &ProgressDialogManager::operationProgress);
  }
  layout->addWidget(progressWidget);
  progressDialog->setWindowTitle(QString("%1 Progress").arg(op->label()));
  progressDialog->setLayout(layout);
  progressDialog->adjustSize();
  progressDialog->show();
  QCoreApplication::processEvents();
}

void ProgressDialogManager::operatorAdded(Operator *op)
{
  QObject::connect(op, &Operator::transformingStarted,
                   this, &ProgressDialogManager::operationStarted);
}

void ProgressDialogManager::dataSourceAdded(DataSource *ds)
{
  QObject::connect(ds, SIGNAL(operatorAdded(Operator*)),
                   this, SLOT(operatorAdded(Operator*)));
}

void ProgressDialogManager::operationProgress(int progress)
{
  // Probably not strictly neccessary in the long run where we have a background thread.
  // But until then we need this call.
  QCoreApplication::processEvents();
}
}
