/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveWebReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"

#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"

#include "WebExportWidget.h"

#include "MainWindow.h"
#include "PythonUtilities.h"
#include "Utilities.h"

#include <cassert>

#include <pqSettings.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QMap>
#include <QMessageBox>
#include <QRegularExpression>
#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace tomviz {

SaveWebReaction::SaveWebReaction(QAction* parentObject, MainWindow* mainWindow)
  : pqReaction(parentObject), m_mainWindow(mainWindow)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void SaveWebReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

void SaveWebReaction::onTriggered()
{
  WebExportWidget dialog;
  if (dialog.exec() == QDialog::Accepted) {

    QString lastUsedExt;
    // Load the most recently used file extensions from QSettings, if available.
    auto settings = pqApplicationCore::instance()->settings();
    QString defaultFileName = "tomviz.html";
    if (settings->contains("web/exportFilename")) {
      defaultFileName = settings->value("web/exportFilename").toString();
    }

    QStringList filters;
    filters << "HTML (*.html)";

    QFileDialog fileDialog(nullptr, "Save Web Export:");
    fileDialog.setFileMode(QFileDialog::AnyFile);
    fileDialog.setNameFilters(filters);
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.selectFile(defaultFileName);

    if (fileDialog.exec() != QDialog::Accepted) {
      return;
    }

    auto args = dialog.getKeywordArguments();
    auto filePath = fileDialog.selectedFiles()[0];
    QFileInfo info(filePath);
    auto filename = info.fileName();
    args.insert("htmlFilePath", QVariant(filePath));
    settings->setValue("web/exportFilename", QVariant(filename));
    this->saveWeb(args);
  }
}

void SaveWebReaction::saveWeb(QMap<QString, QVariant> kwargsMap)
{
  Python::initialize();

  auto messageDialog = new QMessageBox(this->m_mainWindow);
  messageDialog->setStandardButtons(QMessageBox::NoButton);
  messageDialog->setWindowTitle("Web export in progress");
  messageDialog->setText("Saving Web Export. This may take some time, during "
                         "which time the application will be unresponsive.");
  messageDialog->show();

  // A little QTimer hackery to ensure our message dialog is rendered before
  // we lose the main thread. For what ever reason
  // QCoreApplication::processEvents(...)
  // doesn't help us here.
  QTimer::singleShot(200, [kwargsMap, messageDialog]() {
    Python python;
    Python::Module webModule = python.import("tomviz.web");
    if (!webModule.isValid()) {
      qCritical() << "Failed to import tomviz.web module.";
    }

    Python::Function webExport = webModule.findFunction("web_export");
    if (!webExport.isValid()) {
      qCritical() << "Unable to locate webExport.";
    }

    Python::Tuple args(0);
    Python::Dict kwargs;

    // Fill kwargs
    foreach (const QString& str, kwargsMap.keys()) {
      kwargs.set(str, toVariant(kwargsMap.value(str)));
    }

    Python::Object result = webExport.call(args, kwargs);
    if (!result.isValid()) {
      qCritical("Failed to execute the script.");
    }

    messageDialog->accept();
    messageDialog->deleteLater();
  });
}

} // end of namespace tomviz
