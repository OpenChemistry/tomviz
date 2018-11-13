/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <string>

namespace tomviz {

static const int MAX_ITEMS = 10;

namespace {

QJsonObject loadSettings()
{
  // Load the recent files from our saved state.
  auto settings = pqApplicationCore::instance()->settings();
  // Remove a key that can interfere/double count.
  settings->remove("RecentlyUsedResourcesList");
  auto recent = settings->value("recentFiles").toByteArray();
  auto doc = QJsonDocument::fromJson(recent);
  if (!doc.isNull() && doc.isObject()) {
    return doc.object();
  } else {
    QJsonObject json;
    json["readers"] = QJsonArray();
    json["molecules"] = QJsonArray();
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
  QJsonArray molecules;
  if (json.contains("molecules") && json["molecules"].isArray()) {
    molecules = json["molecules"].toArray();
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
  if (molecules.size() > MAX_ITEMS) {
    // We need to prune the list to 10.
    while (molecules.size() > MAX_ITEMS) {
      molecules.removeLast();
    }
  }
  json["readers"] = readers;
  json["states"] = states;
  json["molecules"] = molecules;
  QJsonDocument doc(json);

  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("recentFiles", doc.toJson(QJsonDocument::Compact));
  // Remove a key that can interfere/double count.
  settings->remove("RecentlyUsedResourcesList");
}

} // namespace

RecentFilesMenu::RecentFilesMenu(QMenu& menu, QObject* p) : QObject(p)
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
  auto fileNames = dataSource->fileNames();
  if (fileNames.size() < 1) {
    return;
  }

  readerJson["fileNames"] = QJsonArray::fromStringList(fileNames);

  // Remove the file if it is already in the list
  for (int i = readerList.size() - 1; i >= 0; --i) {
    if (readerList[i].toObject()["fileNames"].toArray()[0] ==
        readerJson["fileNames"].toArray()[0]) {
      readerList.removeAt(i);
    }
  }
  readerList.push_front(readerJson);
  settings["readers"] = readerList;
  saveSettings(settings);
}

void RecentFilesMenu::pushMoleculeReader(MoleculeSource* moleculeSource)
{
  auto settings = loadSettings();
  auto readerList = settings["molecules"].toArray();
  QJsonObject readerJson;

  readerJson["fileName"] = moleculeSource->fileName();

  // Remove the file if it is already in the list
  for (int i = readerList.size() - 1; i >= 0; --i) {
    if (readerList[i].toObject()["fileName"] == readerJson["fileName"]) {
      readerList.removeAt(i);
    }
  }
  readerList.push_front(readerJson);
  settings["molecules"] = readerList;
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
  menu->setToolTipsVisible(true);

  auto json = loadSettings();

  // There are no entries, make it clear in the recent files menu.
  if (!json.contains("readers") && !json.contains("states")) {
    auto actn = menu->addAction("Empty");
    actn->setEnabled(false);
    return;
  }

  // We have something, let's populate the recent files and/or state files.
  if (json["readers"].toArray().size() > 0) {
    menu->addAction("Data files")->setEnabled(false);
  }

  int index = 0;

  foreach (QJsonValue file, json["readers"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      auto fileNamesArray = object["fileNames"].toArray();
      QStringList fileNames;
      foreach (file, fileNamesArray) {
        fileNames << file.toString("<bug>");
      }
      QString label = fileNames[0];
      QString toolTip;
      // Truncate the number of files in the tooltip to 15
      const auto maxEntries = 15;
      if (fileNames.size() > maxEntries) {
        toolTip = (fileNames.mid(0, maxEntries) << QString("...")).join("\n");
      } else {
        toolTip = fileNames.join("\n");
      }
      auto actn =
        menu->addAction(QIcon(":/pqWidgets/Icons/pqInspect22.png"), label);
      actn->setToolTip(toolTip);
      actn->setData(index);
      connect(actn, &QAction::triggered, [this, actn, fileNames]() {
        dataSourceTriggered(actn, fileNames);
      });
      ++index;
    }
  }

  // We have something, let's populate the recent files and/or state files.
  if (json["molecules"].toArray().size() > 0) {
    menu->addAction("Molecule files")->setEnabled(false);
  }

  index = 0;

  foreach (QJsonValue file, json["molecules"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      QString label = object["fileName"].toString("<bug>");
      auto actn = menu->addAction(QIcon(":/icons/gradient_opacity.png"), label);
      actn->setData(index);
      connect(actn, &QAction::triggered,
              [this, actn, label]() { moleculeSourceTriggered(actn, label); });
      ++index;
    }
  }

  if (json["states"].toArray().size() > 0) {
    menu->addAction("State files")->setEnabled(false);
  }

  foreach (QJsonValue file, json["states"].toArray()) {
    if (file.isObject()) {
      auto object = file.toObject();
      auto actn = menu->addAction(QIcon(":/icons/tomviz.png"),
                                  object["fileName"].toString("<bug>"));
      actn->setData(object["fileName"].toString("<bug>"));
      connect(actn, SIGNAL(triggered()), SLOT(stateTriggered()));
    }
  }
}

void RecentFilesMenu::dataSourceTriggered(QAction* actn, QStringList fileNames)
{
  // Check the files actually exists, remove the recent entry if not.
  int missingIdx = -1;
  for (auto i = 0; i < fileNames.size(); ++i) {
    auto file = fileNames[i];
    if (!QFileInfo::exists(file)) {
      QMessageBox::warning(tomviz::mainWidget(), "Error",
                           QString("The file '%1' does not exist").arg(file));
      missingIdx = i;
      break;
    }
  }
  if (missingIdx != -1) {
    int index = actn->data().toInt();
    auto json = loadSettings();
    auto readers = json["readers"].toArray();
    readers.removeAt(index);
    json["readers"] = readers;
    saveSettings(json);
    return;
  }

  LoadDataReaction::loadData(fileNames);
}

void RecentFilesMenu::moleculeSourceTriggered(QAction* actn, QString fileName)
{
  // Check the files actually exists, remove the recent entry if not.
  bool missing = false;
  if (!QFileInfo::exists(fileName)) {
    QMessageBox::warning(tomviz::mainWidget(), "Error",
                         QString("The file '%1' does not exist").arg(fileName));
    missing = true;
  }

  if (missing) {
    int index = actn->data().toInt();
    auto json = loadSettings();
    auto readers = json["molecules"].toArray();
    readers.removeAt(index);
    json["molecules"] = readers;
    saveSettings(json);
    return;
  }

  LoadDataReaction::loadMolecule(fileName);
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
} // namespace tomviz
