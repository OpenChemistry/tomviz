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
#include "pqFileDialog.h"

#include "WebExportWidget.h"

#include "PythonUtilities.h"
#include "Utilities.h"

#include <cassert>

#include <QDialog>
#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>

namespace tomviz {

SaveWebReaction::SaveWebReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

SaveWebReaction::~SaveWebReaction()
{
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
        this->saveWeb(dialog.getOutputPath(), dialog.getExportType(), dialog.getDeltaPhi(), dialog.getDeltaTheta());
    }
}

bool SaveWebReaction::saveWeb(const QString& filename, int type, int deltaPhi, int deltaTheta)
{
  cout << "Generate" << filename.toLatin1().data() << endl;
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

  Python::Dict kwargs;
  Python::Tuple args(4);
  args.set(0, toVariant(QVariant(filename)));
  args.set(1, toVariant(QVariant(type)));
  args.set(2, toVariant(QVariant(deltaPhi)));
  args.set(3, toVariant(QVariant(deltaTheta)));

  Python::Object result = webExport.call(args, kwargs);
  if (!result.isValid()) {
    qCritical("Failed to execute the script.");
    return false;
  }

  return true;
}

} // end of namespace tomviz
