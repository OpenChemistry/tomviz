/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ProgressDialogManager.h"

#include "DataSource.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "Pipeline.h"
#include "PipelineManager.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMainWindow>
#include <QMap>
#include <QProgressBar>
#include <QStatusBar>
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

ProgressDialogManager::~ProgressDialogManager() {}

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
    QObject::connect(op, &Operator::progressStepChanged, progressBar,
                     &QProgressBar::setValue);
    QObject::connect(op, &Operator::totalProgressStepsChanged, progressBar,
                     &QProgressBar::setMaximum);
    QObject::connect(op, &Operator::progressStepChanged, this,
                     &ProgressDialogManager::operationProgress);
    QObject::connect(
      op, &Operator::progressMessageChanged, progressDialog,
      [progressDialog, op](const QString& message) {
        if (!message.isNull()) {
          QString title = QString("%1 Progress").arg(op->label());
          if (!message.isEmpty()) {
            title = QString("%1 Progress - %2").arg(op->label()).arg(message);
          }
          progressDialog->setWindowTitle(title);
        }
      });

    connect(op, &Operator::progressMessageChanged, this,
            &ProgressDialogManager::showStatusBarMessage);

    layout->addWidget(progressBar);
  }
  layout->addWidget(progressWidget);
  if (op->supportsCancelingMidTransform()) {
    // Unless the widget has custom progress handling, you can't cancel it.
    QDialogButtonBox* dialogButtons = new QDialogButtonBox(
      QDialogButtonBox::Cancel, Qt::Horizontal, progressDialog);
    layout->addWidget(dialogButtons);
    QObject::connect(progressDialog, &QDialog::rejected, op,
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
  progressDialog->resize(500, height);
  progressDialog->show();
  QCoreApplication::processEvents();
}

void ProgressDialogManager::operatorAdded(Operator* op)
{
  // Need to ensure that if we are using the docker executor we use a
  // DirectQueued
  // connection here, otherwise we will deadlock as the sender and receiver will
  // will have the same thread affinity.
  std::function<void()> connectTransformingStarted = [op, this]() {
    auto connectionType = Qt::BlockingQueuedConnection;
    if (op->dataSource()->pipeline()->executionMode() !=
        Pipeline::ExecutionMode::Threaded) {
      connectionType = Qt::DirectConnection;
    }

    connect(op, &Operator::transformingStarted, this,
            &ProgressDialogManager::operationStarted, connectionType);
  };
  connectTransformingStarted();

  // Recreate the connection with the correct type if the execution mode is
  // changed.
  connect(&PipelineManager::instance(), &PipelineManager::executionModeUpdated,
          op, [this, connectTransformingStarted, op]() {
            disconnect(op, &Operator::transformingStarted, this,
                       &ProgressDialogManager::operationStarted);
            connectTransformingStarted();
          });

  connect(
    op,
    static_cast<void (Operator::*)(DataSource*)>(&Operator::newChildDataSource),
    this, &ProgressDialogManager::dataSourceAdded);
}

void ProgressDialogManager::dataSourceAdded(DataSource* ds)
{
  QObject::connect(ds, SIGNAL(operatorAdded(Operator*)), this,
                   SLOT(operatorAdded(Operator*)));
}

void ProgressDialogManager::operationProgress(int) {}

void ProgressDialogManager::showStatusBarMessage(const QString& message)
{
  this->mainWindow->statusBar()->showMessage(message, 3000);
}
} // namespace tomviz
