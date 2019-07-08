/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetModel.h"
#include "Utilities.h"

#include <pqPresetToPixmap.h>
#include <pqSettings.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QPair>
#include <QPixmap>
#include <QSize>

#include <vtkSMProxy.h>
#include <vtkScalarsToColors.h>
#include <vtk_jsoncpp.h>

namespace tomviz {

PresetModel::PresetModel(QObject* parent) : QAbstractTableModel(parent)
{
  auto settings = pqApplicationCore::instance()->settings();
  auto presetColors = settings->value("presetColors").toByteArray();
  auto doc = QJsonDocument::fromJson(presetColors);
  if (doc.isNull()) {
    loadFromFile();
  } else {
    m_Presets = doc.array();
  }
}

int PresetModel::rowCount(const QModelIndex& id) const
{
  return id.isValid() ? 0 : m_Presets.size();
}

int PresetModel::columnCount(const QModelIndex& /*parent*/) const
{
  return 1;
}

QVariant PresetModel::data(const QModelIndex& index, int role) const
{
  switch (role) {
    case Qt::DisplayRole:
      return m_Presets[index.row()].toObject().value("name");

    case Qt::DecorationRole:
      auto pixmap = render(m_Presets[index.row()].toObject());
      return pixmap;

      /*  case Qt::TextAlignmentRole:
          return Qt::AlignLeft + Qt::AlignVCenter;*/
  }

  return QVariant();
}

QVariant PresetModel::headerData(int, Qt::Orientation, int) const
{
  return QVariant();
}

void PresetModel::setRow(const QModelIndex& index)
{
  m_row = index.row();
}

void PresetModel::updateRow()
{
  m_row = m_Presets.size() - 1;
}

QString PresetModel::presetName()
{
  return m_Presets[m_row].toObject().value("name").toString();
}

QJsonObject PresetModel::jsonObject()
{
  QJsonObject pqPreset(m_Presets[m_row].toObject());
  pqPreset.insert("RGBPoints", pqPreset["colors"]);
  pqPreset.insert("ColorSpace", pqPreset["colorSpace"]);
  return pqPreset;
}

void PresetModel::changePreset(const QModelIndex& index)
{
  emit setRow(index);
  emit applyPreset();
}

void PresetModel::addNewPreset(const QJsonObject& newPreset)
{
  m_Presets.push_back(newPreset);
  updateRow();
  saveSettings();
  beginResetModel();
  endResetModel();
}

QPixmap PresetModel::render(const QJsonObject& newPreset) const
{
  QJsonObject pqPreset(newPreset);
  pqPreset.insert("RGBPoints", pqPreset["colors"]);
  pqPreset.insert("ColorSpace", pqPreset["colorSpace"]);

  QJsonDocument doc(pqPreset);
  QString preset(doc.toJson(QJsonDocument::Compact));

  Json::Value colors;
  Json::Reader reader;
  reader.parse(preset.toLatin1().data(), colors);

  pqPresetToPixmap PixMapRenderer;
  return PixMapRenderer.render(colors, QSize(135, 20));
}

void PresetModel::saveSettings()
{
  QJsonDocument doc(m_Presets);
  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("presetColors", doc.toJson(QJsonDocument::Compact));
}

void PresetModel::loadFromFile()
{
  QString path = QApplication::applicationDirPath() +
                 "/../share/tomviz/matplotlib_cmaps.json";
  QFile file(path);
  if (!file.exists()) {
// On OSX the above doesn't work in a build tree.  It is fine
// for superbuilds, but the following is needed in the build tree
// since the executable is three levels down in bin/tomviz.app/Contents/MacOS/
#ifdef __APPLE__
    path = QApplication::applicationDirPath() +
           "/../../../../share/tomviz/matplotlib_cmaps.json";
#else
    path = "";
#endif
  }

  file.open(QIODevice::ReadOnly);
  QString defaults(file.readAll());
  file.close();

  QJsonDocument doc = QJsonDocument::fromJson(defaults.toUtf8());
  QJsonArray objects = doc.array();
  for (auto value : objects) {
    QJsonObject obj = value.toObject();
    QJsonObject nextDefault{
      { "name", obj["Name"] },
      { "colorSpace", obj.contains("ColorSpace")
	  ? obj["ColorSpace"] : QJsonValue("Diverging") },
      { "colors", obj["RGBPoints"] },
    };
    m_Presets.push_back(nextDefault);
  }
  saveSettings();
}

void PresetModel::deletePreset(const QModelIndex& index)
{
  m_Presets.removeAt(index.row());
  if (m_row >= m_Presets.size()) {
    updateRow();
  }
  saveSettings();
  beginResetModel();
  endResetModel();
}
} // namespace tomviz
