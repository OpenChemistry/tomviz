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
#include "RecentFilesMenu.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "SaveLoadStateReaction.h"
#include "Utilities.h"
#include "pqPipelineSource.h"
#include "pqSettings.h"
#include "vtkNew.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSmartPointer.h"

#include <QDebug>
#include <QMenu>
#include <sstream>
#include <string>

namespace tomviz {

static const int MAX_ITEMS = 10;

void get_settings(pugi::xml_document& doc)
{
  QSettings* settings = pqApplicationCore::instance()->settings();
  QString recent = settings->value("recentFiles").toString();
  if (recent.isEmpty() || !doc.load(recent.toUtf8().data()) ||
      !doc.child("tomvizRecentFilesMenu")) {
    doc.append_child("tomvizRecentFilesMenu");
  }
}

void save_settings(pugi::xml_document& doc)
{
  // trim the list.
  pugi::xml_node root = doc.root();
  std::vector<pugi::xml_node> to_remove;
  int counter = 0;
  for (pugi::xml_node node = root.child("DataReader"); node;
       node = node.next_sibling("DataReader"), counter++) {
    if (counter >= MAX_ITEMS) {
      to_remove.push_back(node);
    }
  }
  counter = 0;
  for (pugi::xml_node node = root.child("State"); node;
       node = node.next_sibling("State"), counter++) {
    if (counter >= MAX_ITEMS) {
      to_remove.push_back(node);
    }
  }
  for (size_t cc = 0; cc < to_remove.size(); cc++) {
    root.remove_child(to_remove[cc]);
  }

  std::ostringstream stream;
  doc.save(stream);
  QSettings* settings = pqApplicationCore::instance()->settings();
  settings->setValue("recentFiles", stream.str().c_str());
}

RecentFilesMenu::RecentFilesMenu(QMenu& menu, QObject* parentObject)
  : Superclass(parentObject)
{
  this->connect(&menu, SIGNAL(aboutToShow()), SLOT(aboutToShowMenu()));
}

RecentFilesMenu::~RecentFilesMenu()
{
}

void RecentFilesMenu::pushDataReader(DataSource* dataSource,
                                     vtkSMProxy* readerProxy)
{
  // Add non-proxy based readers separately.
  if (!readerProxy) {
    pugi::xml_document settings;
    get_settings(settings);

    pugi::xml_node root = settings.root();
    QByteArray labelBytes = dataSource->filename().toLatin1();
    const char* filename = labelBytes.data();

    for (pugi::xml_node node = root.child("DataReader"); node;
         node = node.next_sibling("DataReader")) {
      if (strcmp(node.attribute("filename0").as_string(""), filename) == 0) {
        root.remove_child(node);
        break;
      }
    }

    pugi::xml_node node = root.prepend_child("DataReader");
    node.append_attribute("filename0").set_value(filename);
    save_settings(settings);
    return;
  }

  const char* pname = vtkSMCoreUtilities::GetFileNameProperty(readerProxy);
  // Only add to list if the data source is associated with file(s)
  if (pname) {
    pugi::xml_document settings;
    get_settings(settings);

    pugi::xml_node root = settings.root();
    QByteArray labelBytes = dataSource->filename().toLatin1();
    const char* filename = labelBytes.data();

    for (pugi::xml_node node = root.child("DataReader"); node;
         node = node.next_sibling("DataReader")) {
      if (strcmp(node.attribute("filename0").as_string(""), filename) == 0) {
        root.remove_child(node);
        break;
      }
    }

    pugi::xml_node node = root.prepend_child("DataReader");
    node.append_attribute("filename0").set_value(filename);
    node.append_attribute("xmlgroup").set_value(readerProxy->GetXMLGroup());
    node.append_attribute("xmlname").set_value(readerProxy->GetXMLName());
    if (dataSource->isImageStack()) {
      node.append_attribute("stack").set_value(true);
    }
    tomviz::serialize(readerProxy, node);

    save_settings(settings);
  }
}

void RecentFilesMenu::pushStateFile(const QString& filename)
{
  pugi::xml_document settings;
  get_settings(settings);

  pugi::xml_node root = settings.root();
  for (pugi::xml_node node = root.child("State"); node;
       node = node.next_sibling("State")) {
    if (filename == node.attribute("filename").as_string("")) {
      root.remove_child(node);
      break;
    }
  }

  pugi::xml_node node = root.prepend_child("State");
  node.append_attribute("filename").set_value(filename.toLatin1().data());
  save_settings(settings);
}

void RecentFilesMenu::aboutToShowMenu()
{
  QMenu* menu = qobject_cast<QMenu*>(this->sender());
  Q_ASSERT(menu);
  menu->clear();

  pugi::xml_document settings;
  get_settings(settings);

  pugi::xml_node root = settings.root();
  if (!root.child("DataReader") && !root.child("State")) {
    QAction* actn = menu->addAction("Empty");
    actn->setEnabled(false);
    return;
  }

  bool header_added = false;
  int index = 0;
  for (pugi::xml_node node = root.child("DataReader"); node;
       node = node.next_sibling("DataReader")) {
    if (header_added == false) {
      QAction* actn = menu->addAction("Datasets");
      actn->setEnabled(false);
      header_added = true;
    }
    QFileInfo checkFile(node.attribute("filename0").as_string("<bug>"));
    bool stack = node.attribute("stack").as_bool(false);
    if (checkFile.exists() || stack) {
      QAction* actn =
        menu->addAction(QIcon(":/pqWidgets/Icons/pqInspect22.png"),
                        node.attribute("filename0").as_string("<bug>"));
      actn->setData(index);
      this->connect(actn, &QAction::triggered, [this, actn, stack]() {
        dataSourceTriggered(actn, stack);
      });
    }
    index++;
  }

  header_added = false;
  for (pugi::xml_node node = root.child("State"); node;
       node = node.next_sibling("State")) {
    if (header_added == false) {
      QAction* actn = menu->addAction("State files");
      actn->setEnabled(false);
      header_added = true;
    }
    QFileInfo checkFile(node.attribute("filename").as_string("<bug>"));
    if (checkFile.exists()) {
      QAction* actn =
        menu->addAction(QIcon(":/icons/tomviz.png"),
                        node.attribute("filename").as_string("<bug>"));
      actn->setData(node.attribute("filename").as_string("<bug>"));
      this->connect(actn, SIGNAL(triggered()), SLOT(stateTriggered()));
    }
    index++;
  }
}

void RecentFilesMenu::dataSourceTriggered(QAction* actn, bool stack)
{

  QFileInfo checkFile(actn->iconText());
  if (!checkFile.exists() && !stack) {
    // This should never happen since the checks in aboutToShowMenu should
    // prevent it, but just in case...
    qWarning() << "Error: file '" << actn->iconText() << "' does not exist.";
    return;
  }

  int index = actn->data().toInt();
  pugi::xml_document settings;
  get_settings(settings);
  pugi::xml_node root = settings.root();

  for (pugi::xml_node node = root.child("DataReader"); node;
       node = node.next_sibling("DataReader"), --index) {
    if (index == 0) {
      if (node.attribute("xmlgroup").empty()) {
        // Special node for EMDs that have no proxy.
        if (LoadDataReaction::createDataSourceLocal(
              node.attribute("filename0").as_string())) {
          // reorder the nodes to move the recently opened file to the top.
          root.prepend_copy(node);
          root.remove_child(node);
          save_settings(settings);
          return;
        }
        // failed to create reader, remove the node.
        root.remove_child(node);
        save_settings(settings);
        return;
      }
      vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
      vtkSmartPointer<vtkSMProxy> reader;
      reader.TakeReference(
        pxm->NewProxy(node.attribute("xmlgroup").as_string(),
                      node.attribute("xmlname").as_string()));
      if (tomviz::deserialize(reader, node)) {
        reader->UpdateVTKObjects();
        vtkSMSourceProxy::SafeDownCast(reader)->UpdatePipelineInformation();
        if (LoadDataReaction::createDataSource(reader)) {
          // reorder the nodes to move the recently opened file to the top.
          root.prepend_copy(node);
          root.remove_child(node);
          save_settings(settings);
          return;
        }
        // If the user pressed 'Cancel' on the reader properties dialog,
        // don't remove the node
        return;
      }
      // failed to create reader, remove the node.
      root.remove_child(node);
      save_settings(settings);
      return;
    }
  }
}

void RecentFilesMenu::stateTriggered()
{
  QAction* actn = qobject_cast<QAction*>(this->sender());
  Q_ASSERT(actn);

  QFileInfo checkFile(actn->iconText());
  if (!checkFile.exists()) {
    // This should never happen since the checks in aboutToShowMenu should
    // prevent it, but just in case...
    qWarning() << "Error: file '" << actn->iconText() << "' does not exist.";
    return;
  }

  QString filename = actn->data().toString();
  if (SaveLoadStateReaction::loadState(filename)) {
    // the above call will ensure that the file name moves to top of the list
    // since it calls pushStateFile() on success.
    return;
  }

  // remove the item from the recent state files list.
  pugi::xml_document settings;
  get_settings(settings);
  pugi::xml_node root = settings.root();
  for (pugi::xml_node node = root.child("State"); node;
       node = node.next_sibling("State")) {
    if (filename == node.attribute("filename").as_string()) {
      root.remove_child(node);
      save_settings(settings);
      break;
    }
  }
}
}
