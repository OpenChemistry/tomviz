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

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMainWindow>
#include <QMap>
#include <QProgressBar>
#include <QVBoxLayout>

#include <cassert>
#include <iostream>

namespace tomviz {

ProgressDialogManager::ProgressDialogManager(QMainWindow* mw)
  : Superclass(mw), mainWindow(mw)
{
  ModuleManager& mm = ModuleManager::instance();
  QObject::connect(&mm, SIGNAL(dataSourceAdded(DataSource*)), this,
                   SLOT(dataSourceAdded(DataSource*)));
}

ProgressDialogManager::~ProgressDialogManager()
{
}

void ProgressDialogManager::operationStarted()
{
  QDialog* progressDialog = new QDialog(this->mainWindow);
  progressDialog->setAttribute(Qt::WA_DeleteOnClose);

  Operator* op = qobject_cast<Operator*>(this->sender());
  QObject::connect(op, &Operator::transformingDone, progressDialog,
                   &QDialog::accept);

  // We have to check after we have connected to the signal as otherwise we
  // might miss the state transition as its occurring on another thread.
  if (op->isFinished()) {
    progressDialog->accept();
    progressDialog->deleteLater();
    return;
  }

  QLayout* layout = new QVBoxLayout();
  QWidget* progressWidget = op->getCustomProgressWidget(progressDialog);
  if (progressWidget == nullptr) {
    QProgressBar* progressBar = new QProgressBar(progressDialog);
    progressBar->setMinimum(0);
    progressBar->setMaximum(op->totalProgressSteps());
    progressWidget = progressBar;
    QObject::connect(op, &Operator::updateProgress, progressBar,
                     &QProgressBar::setValue);
    QObject::connect(op, &Operator::totalProgressStepsChanged, progressBar,
                     &QProgressBar::setMaximum);
    QObject::connect(op, &Operator::updateProgress, this,
                     &ProgressDialogManager::operationProgress);
    layout->addWidget(progressBar);
  }
  layout->addWidget(progressWidget);
  if (op->supportsCancelingMidTransform()) {
    // Unless the widget has custom progress handling, you can't cancel it.
    QDialogButtonBox* dialogButtons = new QDialogButtonBox(
      QDialogButtonBox::Cancel, Qt::Horizontal, progressDialog);
    layout->addWidget(dialogButtons);
    QObject::connect(dialogButtons, &QDialogButtonBox::rejected, op,
                     &Operator::cancelTransform);
    QObject::connect(dialogButtons, &QDialogButtonBox::rejected, progressDialog,
                     &QDialog::reject);
  }
  progressDialog->setWindowTitle(QString("%1 Progress").arg(op->label()));
  progressDialog->setLayout(layout);
  progressDialog->adjustSize();
  // Increase size of dialog so we can see title, not sure there is a better
  // way.
  auto height = progressDialog->height();
  progressDialog->resize(300, height);
  progressDialog->show();
  QCoreApplication::processEvents();
}

void ProgressDialogManager::operatorAdded(Operator* op)
{
  QObject::connect(op, &Operator::transformingStarted, this,
                   &ProgressDialogManager::operationStarted);
}

void ProgressDialogManager::dataSourceAdded(DataSource* ds)
{
  QObject::connect(ds, SIGNAL(operatorAdded(Operator*)), this,
                   SLOT(operatorAdded(Operator*)));
}

void ProgressDialogManager::operationProgress(int)
{
  // Probably not strictly neccessary in the long run where we have a background
  // thread, until then we need this call.
  QCoreApplication::processEvents();
}
}
