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

#include <pqSettings.h>

#include <QDebug>
#include <QMenu>
#include <QMessageBox>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <string>

namespace tomviz {

static const int MAX_ITEMS = 10;

namespace{

QJsonObject loadSettings()
{
  // Load the recent files from our saved state.
  auto settings = pqApplicationCore::instance()->settings();
  auto recent = settings->value("recentFiles").toByteArray();
  auto doc = QJsonDocument::fromJson(recent);
  if (!doc.isNull() && doc.isObject()) {
    return doc.object();
  } else {
    QJsonObject json;
    json["readers"] = QJsonArray();
    json["states"] = QJsonArray();
    return QJsonObject();
  }
}

void saveSettings(QJsonObject json)
{
  // Save recent file list to our saved state, prune lists to be <= 10 entries.
  QJsonArray readers;
  if (json.contains("readers") && json["readers"].isArray()) {
    readers = json["readers"].toArray();
  }
  QJsonArray states;
  if (json.contains("states") && json["states"].isArray()) {
    states = json["states"].toArray();
  }
  if (readers.size() > MAX_ITEMS) {
    // We need to prune the list to 10.
    while (readers.size() > MAX_ITEMS) {
      readers.removeLast();
    }
  }
  if (states.size() > MAX_ITEMS) {
    // We need to prune the list to 10.
    while (states.size() > MAX_ITEMS) {
      states.removeLast();
    }
  }
  json["readers"] = readers;
  json["states"] = states;
  QJsonDocument doc(json);

  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("recentFiles", doc.toJson(QJsonDocument::Compact));
}

}

RecentFilesMenu::RecentFilesMenu(QMenu& menu, QObject* p)
  : QObject(p)
{
  connect(&menu, SIGNAL(aboutToShow()), SLOT(aboutToShowMenu()));
}

RecentFilesMenu::~RecentFilesMenu() = default;

void RecentFilesMenu::pushDataReader(DataSource* dataSource)
{
  // Add non-proxy based readers separately.
  auto settings = loadSettings();
  auto readerList = settings["readers"].toArray();
  QJsonObject readerJson =
    QJsonObject::fromVariantMap(dataSource->readerProperties());
  readerJson["fileName"] = dataSource->fileName();
  readerJson["stack"] = dataSource->isImageStack();

  // Remove the file if it is already in the list
  for (int i = readerList.size() - 1; i >= 0; --i) {
    if (readerList[i].toObject()["fileName"] == readerJson["fileName"]) {
      readerList.removeAt(i);
    }
  }
  readerList.push_front(readerJson);
  settings["readers"] = readerList;
  saveSettings(settings);
}

void RecentFilesMenu::pushStateFile(const QString& fileName)
{
  auto settings = loadSettings();
  auto stateList = settings["states"].toArray();
  QJsonObject stateJson;
  stateJson["fileName"] = fileName;
  // Remove the file if it is already in the list
  for (int i = stateList.size() - 1; i >= 0; --i) {
    if (stateList[i].toObject()["fileName"] == stateJson["fileName"]) {
      stateList.removeAt(i);
    }
  }
  stateList.push_front(stateJson);
  settings["states"] = stateList;
  saveSettings(settings);
}

void RecentFilesMenu::aboutToShowMenu()
{
  auto menu = qobject_cast<QMenu*>(sender());
  Q_ASSERT(menu);
  menu->clear();

  auto json = loadSettings();

  // There are no entries, make it clear in the recent files menu.
  if (!json.contains("readers") && !json.contains("states")) {
    auto actn = menu->addAction("Empty");
    actn->setEnabled(false);
    return;
  }

  // We have something, let's populate the recent files and/or state files.
  bool headerAdded = false;
  int index = 0;
  foreach(QJsonValue file, json["readers"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      if (headerAdded == false) {
        auto actn = menu->addAction("Data files");
        actn->setEnabled(false);
        headerAdded = true;
      }
      bool stack = object["stack"].toBool(false);
      auto actn =
        menu->addAction(QIcon(":/pqWidgets/Icons/pqInspect22.png"),
                        object["fileName"].toString("<bug>"));
      actn->setData(index);
      connect(actn, &QAction::triggered, [this, actn, stack]() {
        dataSourceTriggered(actn, stack);
      });
      ++index;
    }
  }

  headerAdded = false;
  foreach(QJsonValue file, json["states"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      if (headerAdded == false) {
        auto actn = menu->addAction("State files");
        actn->setEnabled(false);
        headerAdded = true;
      }
      auto actn =
        menu->addAction(QIcon(":/icons/tomviz.png"),
                        object["fileName"].toString("<bug>"));
      actn->setData(object["fileName"].toString("<bug>"));
      connect(actn, SIGNAL(triggered()), SLOT(stateTriggered()));
    }
  }
}

void RecentFilesMenu::dataSourceTriggered(QAction* actn, bool stack)
{
  int index = actn->data().toInt();
  auto json = loadSettings();
  QJsonArray readers;
  if (json["readers"].isArray()) {
    readers = json["readers"].toArray();
  } else {
    return;
  }

  QJsonObject file;
  if (index < readers.size() && readers.at(index).isObject()) {
    file = readers.at(index).toObject();
    // Check the file actually exists, remove it if not.
    if (!QFileInfo::exists(file["fileName"].toString()) && !stack) {
      QMessageBox::warning(
        tomviz::mainWidget(), "Error",
        QString("The file '%1' does not exist").arg(actn->iconText()));
      readers.removeAt(index);
      json["readers"] = readers;
      saveSettings(json);
      return;
    }
    // Otherwise attempt to load the file, and bump it to the top of the list
    // if it is loaded successfully.
    auto ds = LoadDataReaction::loadData(file["fileName"].toString(), true,
                                         false, false);
    readers.removeAt(index);
    if (ds) {
      readers.prepend(file);
    }
    json["readers"] = readers;
    saveSettings(json);
  }
}

void RecentFilesMenu::stateTriggered()
{
  auto actn = qobject_cast<QAction*>(sender());
  Q_ASSERT(actn);

  QString fileName = actn->data().toString();

  if (QFileInfo::exists(actn->iconText())) {
    if (SaveLoadStateReaction::loadState(fileName)) {
      // the above call will ensure that the file name moves to top of the list
      // since it calls pushStateFile() on success.
      return;
    }
  } else {
    // If the user tried to open a recent file that no longer exists, remove
    // it from the recent files.
    QMessageBox::warning(
      tomviz::mainWidget(), "Error",
      QString("The file '%1' does not exist").arg(actn->iconText()));
  }

  // Remove the item from the recent state files list.
  auto json = loadSettings();
  if (json["states"].isArray()) {
    auto states = json["states"].toArray();
    for (int i = 0; i < states.size(); ++i) {
      if (states[i].toObject()["fileName"].toString() == fileName) {
        states.removeAt(i);
      }
    }
    saveSettings(json);
  }
}
}
