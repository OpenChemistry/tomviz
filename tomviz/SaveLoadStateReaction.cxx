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

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

#include <vtk_pugixml.h>

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
  if (ModuleManager::instance().hasDataSources()) {
    if (QMessageBox::Yes !=
        QMessageBox::warning(tomviz::mainWidget(), "Load State Warning",
                             "Current data and operators will be cleared when "
                             "loading a state file.  Proceed anyway?",
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)) {
      return false;
    }
  }
  QFile openFile(filename);
  if (!openFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open state file.");
    return false;
  }

  QJsonParseError error;
  auto contents = openFile.readAll();
  auto doc = QJsonDocument::fromJson(contents, &error);
  bool legacyStateFile = false;
  if (doc.isNull()) {
    // See if user is trying to load a old XML base state file.
    if (error.error == QJsonParseError::IllegalValue) {
      legacyStateFile = checkForLegacyStateFileFormat(contents);
    }

    // If its a legacy state file we are done.
    if (legacyStateFile) {
      return false;
    }
  }

  if (doc.isObject() &&
      ModuleManager::instance().deserialize(doc.object(),
                                            QFileInfo(filename).dir())) {
    RecentFilesMenu::pushStateFile(filename);
    return true;
  }

  if (!legacyStateFile) {
    QMessageBox::warning(
      tomviz::mainWidget(), "Invalid state file",
      QString("Unable to read state file: %1").arg(error.errorString()));
  }

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

QString SaveLoadStateReaction::extractLegacyStateFileVersion(
  const QByteArray state)
{
  QString fullVersion;
  pugi::xml_document doc;

  if (doc.load_buffer(state.data(), state.size())) {
    pugi::xpath_node version = doc.select_single_node("/tomvizState/version");

    if (version) {
      fullVersion = version.node().attribute("full").value();
    }
  }

  return fullVersion;
}

bool SaveLoadStateReaction::checkForLegacyStateFileFormat(
  const QByteArray state)
{

  auto version = extractLegacyStateFileVersion(state);
  if (version.length() > 0) {
    QString url = "https://github.com/OpenChemistry/tomviz/releases";
    QString versionString = QString("Tomviz %1").arg(version);
    if (!version.contains("-g")) {
      url = QString("https://github.com/OpenChemistry/tomviz/releases/%1")
              .arg(version);
      versionString = QString("<a href=%1>Tomviz %2</a>").arg(url).arg(version);
    }

    QMessageBox versionWarning(tomviz::mainWidget());
    versionWarning.setIcon(QMessageBox::Icon::Warning);
    versionWarning.setTextFormat(Qt::RichText);
    versionWarning.setWindowTitle("Trying to load a legacy state file?");
    versionWarning.setText(
      QString("This state file was written using %1."
              " The format is not supported by the version of Tomviz you are "
              "running. "
              "A compatible version can be downloaded <a href=%2>here<a>")
        .arg(versionString)
        .arg(url));
    versionWarning.exec();
    return true;
  }

  return false;
}

} // end of namespace
