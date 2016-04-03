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

#include "pqFileDialog.h"
#include "pqCoreUtilities.h"
#include <vtk_pugixml.h>

#include "ModuleManager.h"
#include "RecentFilesMenu.h"
#include "vtkSMProxyManager.h"

#include <QtDebug>
#include <QDir>

namespace tomviz
{

SaveLoadStateReaction::SaveLoadStateReaction(QAction* parentObject, bool load)
  : Superclass(parentObject),
    Load(load)
{
}

SaveLoadStateReaction::~SaveLoadStateReaction()
{
}

void SaveLoadStateReaction::onTriggered()
{
  if (this->Load)
  {
    this->loadState();
  }
  else
  {
    this->saveState();
  }
}

bool SaveLoadStateReaction::saveState()
{
  pqFileDialog fileDialog(nullptr, pqCoreUtilities::mainWidget(),
                          tr("Save State File"), QString(),
                          "tomviz state files (*.tvsm);;All files (*)");
  fileDialog.setObjectName("SaveStateDialog");
  fileDialog.setFileMode(pqFileDialog::AnyFile);
  if (fileDialog.exec() == QDialog::Accepted)
  {
    return SaveLoadStateReaction::saveState(fileDialog.getSelectedFiles()[0]);
  }
  return false;
}

bool SaveLoadStateReaction::loadState()
{
  pqFileDialog fileDialog(nullptr, pqCoreUtilities::mainWidget(),
                          tr("Load State File"), QString(),
                          "tomviz state files (*.tvsm);;All files (*)");
  fileDialog.setObjectName("LoadStateDialog");
  fileDialog.setFileMode(pqFileDialog::ExistingFile);
  if (fileDialog.exec() == QDialog::Accepted)
  {
    return SaveLoadStateReaction::loadState(fileDialog.getSelectedFiles()[0]);
  }
  return false;
}

bool SaveLoadStateReaction::loadState(const QString& filename)
{
  pugi::xml_document document;
  if (!document.load_file(filename.toLatin1().data()))
  {
    qCritical() << "Failed to read file (or file not valid xml) :" << filename;
    return false;
  }

  if (ModuleManager::instance().deserialize(document.child("tomvizState"),
                                            QFileInfo(filename).dir()))
  {
    RecentFilesMenu::pushStateFile(filename);
    return true;
  }
  return false;
}

bool SaveLoadStateReaction::saveState(const QString& filename)
{
  pugi::xml_document document;
  pugi::xml_node root = document.append_child("tomvizState");
  root.append_attribute("version").set_value("0.0a");
  root.append_attribute("paraview_version").set_value(
    QString("%1.%2.%3")
        .arg(vtkSMProxyManager::GetVersionMajor())
        .arg(vtkSMProxyManager::GetVersionMinor())
        .arg(vtkSMProxyManager::GetVersionPatch()).toLatin1().data());

  QFileInfo info(filename);

  return (ModuleManager::instance().serialize(root, info.dir()) &&
    document.save_file(/*path*/ filename.toLatin1().data(), /*indent*/ "  "));
}

} // end of namespace
