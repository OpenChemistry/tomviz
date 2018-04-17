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
#include "SaveLoadStateReaction.h"

#include "ModuleManager.h"
#include "RecentFilesMenu.h"
#include "Utilities.h"

#include <vtkSMProxyManager.h>

#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>

#include <QDebug>

namespace tomviz {

SaveLoadStateReaction::SaveLoadStateReaction(QAction* parentObject, bool load)
  : pqReaction(parentObject), m_load(load)
{
}

void SaveLoadStateReaction::onTriggered()
{
  if (m_load) {
    loadState();
  } else {
    saveState();
  }
}

bool SaveLoadStateReaction::saveState()
{
  QFileDialog fileDialog(tomviz::mainWidget(), tr("Save State File"),
                         QString(),
                         "tomviz state files (*.tvsm);;All files (*)");
  fileDialog.setObjectName("SaveStateDialog");
  fileDialog.setAcceptMode(QFileDialog::AcceptSave);
  fileDialog.setFileMode(QFileDialog::AnyFile);
  if (fileDialog.exec() == QDialog::Accepted) {
    QString filename = fileDialog.selectedFiles()[0];
    QString format = fileDialog.selectedNameFilter();
    if (!filename.endsWith(".tvsm") &&
        format == "tomviz state files (*.tvsm)") {
      filename = QString("%1%2").arg(filename, ".tvsm");
    }
    return SaveLoadStateReaction::saveState(filename);
  }
  return false;
}

bool SaveLoadStateReaction::loadState()
{
  QFileDialog fileDialog(tomviz::mainWidget(), tr("Load State File"),
                         QString(),
                         "tomviz state files (*.tvsm);;All files (*)");
  fileDialog.setObjectName("LoadStateDialog");
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  if (fileDialog.exec() == QDialog::Accepted) {
    return SaveLoadStateReaction::loadState(fileDialog.selectedFiles()[0]);
  }
  return false;
}

bool SaveLoadStateReaction::loadState(const QString& filename)
{
  QFile openFile(filename);
  if (!openFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open state file.");
    return false;
  }

  QJsonParseError error;
  auto doc = QJsonDocument::fromJson(openFile.readAll(), &error);
  if (doc.isNull()) {
    qDebug() << "Error:" << error.errorString();
  }

  if (doc.isObject() &&
      ModuleManager::instance().deserialize(doc.object(),
                                            QFileInfo(filename).dir())) {
    RecentFilesMenu::pushStateFile(filename);
    return true;
  }
  qDebug() << "Failed to read state...";
  return false;
}

bool SaveLoadStateReaction::saveState(const QString& fileName, bool interactive)
{
  QFileInfo info(fileName);
  QFile saveFile(fileName);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
    return false;
  }

  QJsonObject state;
  auto success =
    ModuleManager::instance().serialize(state, info.dir(), interactive);
  QJsonDocument doc(state);
  auto writeSuccess = saveFile.write(doc.toJson());
  return success && writeSuccess != -1;
}

} // end of namespace
