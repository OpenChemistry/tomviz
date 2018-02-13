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
#include "SaveWebReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"

#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"

#include "WebExportWidget.h"

#include "PythonUtilities.h"
#include "Utilities.h"

#include <cassert>

#include <pqSettings.h>

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

SaveWebReaction::SaveWebReaction(QAction* parentObject)
  : pqReaction(parentObject)
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
    args->insert("htmlFilePath", QVariant(filePath));
    settings->setValue("web/exportFilename", QVariant(filename));
    this->saveWeb(args);
  }
}

bool SaveWebReaction::saveWeb(QMap<QString, QVariant>* kwargsMap)
{
  Python::initialize();

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
  foreach (const QString& str, kwargsMap->keys()) {
    kwargs.set(str, toVariant(kwargsMap->value(str)));
  }

  Python::Object result = webExport.call(args, kwargs);
  if (!result.isValid()) {
    qCritical("Failed to execute the script.");
    return false;
  }

  return true;
}

} // end of namespace tomviz
