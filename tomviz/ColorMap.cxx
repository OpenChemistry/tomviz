/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ColorMap.h"

#include <pqApplicationCore.h>
#include <pqPresetToPixmap.h>
#include <pqSettings.h>
#include <vtkSMTransferFunctionProxy.h>

#include <vtk_jsoncpp.h>

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace tomviz
{

ColorMap::ColorMap()
{
  auto settings = pqApplicationCore::instance()->settings();
  auto presetColors = settings->value("presetColors").toByteArray();
  auto doc = QJsonDocument::fromJson(presetColors);
  if (doc.isNull() || !doc.isArray()) {
    loadFromFile();
  } else {
    m_presets = doc.array();
    for (auto value : m_presets) {
      QJsonObject obj = value.toObject();
      if (obj["default"].toBool(false)) {
        m_defaultName = obj["name"].toString();
      }
    }
  }
}

ColorMap::~ColorMap() = default;

ColorMap& ColorMap::instance()
{
  static ColorMap theInstance;
  return theInstance;
}

QString ColorMap::defaultPresetName() const
{
  return m_defaultName;
}

QString ColorMap::presetName(int index) const
{
  if (index >= 0 && index < m_presets.size()) {
    return m_presets[index].toObject().value("name").toString();
  } else {
    return "Error";
  }
}

void ColorMap::setPresetName(int index, const QString& name)
{
  if (index >= 0 && index < m_presets.size()) {
    auto json = m_presets[index].toObject();
    json.insert("name", name);
    m_presets[index] = json;
  }
}

void ColorMap::resetToDefaults()
{
  while (!m_presets.isEmpty()) {
    m_presets.removeLast();
  }
  loadFromFile();
}

int ColorMap::addPreset(const QJsonObject& preset)
{
  m_presets.push_back(preset);
  return m_presets.size();
}

bool ColorMap::deletePreset(int index)
{
  if (index >= 0 && index < m_presets.size()) {
    m_presets.removeAt(index);
    return true;
  } else {
    return false;
  }
}

int ColorMap::count() const
{
  return m_presets.size();
}

QPixmap ColorMap::renderPreview(int index) const
{
  if (index < 0 || index >= m_presets.size()) {
    return QPixmap();
  }

  QJsonObject pqPreset(m_presets[index].toObject());
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

void ColorMap::save()
{
  QJsonDocument doc(m_presets);
  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("presetColors", doc.toJson(QJsonDocument::Compact));
}

void ColorMap::applyPreset(int index, vtkSMProxy* proxy)
{
  if (!proxy || index < 0 || index >= m_presets.size()) {
    return;
  }

  QJsonObject pqPreset(m_presets[index].toObject());
  pqPreset.insert("RGBPoints", pqPreset["colors"]);
  pqPreset.insert("ColorSpace", pqPreset["colorSpace"]);

  QJsonDocument doc(pqPreset);
  QString chosen(doc.toJson(QJsonDocument::Compact));
  Json::Value value;
  Json::Reader reader;
  reader.parse(chosen.toLatin1().data(), value);
  vtkSMTransferFunctionProxy::ApplyPreset(proxy, value, true);
}

void ColorMap::applyPreset(const QString& name, vtkSMProxy* proxy)
{
  if (!proxy) {
    return;
  }

  int index = 0;
  for (auto value : m_presets) {
      QJsonObject obj = value.toObject();
      if (obj["name"].toString() == name) {
        applyPreset(index, proxy);
        return;
      }
      ++index;
    }
}

void ColorMap::loadFromFile()
{
  QString path = QApplication::applicationDirPath() +
                 "/../share/tomviz/defaultcolormaps.json";
  QFile file(path);
  if (!file.exists()) {
    // On macOS this won't work in a build tree. The following is needed in the
    // build tree for macOS relative paths to work as expected.
#ifdef __APPLE__
    path = QApplication::applicationDirPath() +
           "/../../../../share/tomviz/defaultcolormaps.json";
    file.setFileName(path);
#endif
  }

  file.open(QIODevice::ReadOnly);
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  QJsonArray objects = doc.array();
  for (auto value : objects) {
    QJsonObject obj = value.toObject();
    bool defaultMap = obj["Name"].toString() == "Plasma" ? true : false;
    QJsonObject nextDefault{
      { "name", obj["Name"] },
      { "colorSpace", obj.contains("ColorSpace") ? obj["ColorSpace"] : QJsonValue("Diverging") },
      { "colors", obj["RGBPoints"] },
      { "default", QJsonValue(defaultMap) }
    };
    m_presets.push_back(nextDefault);
  }
}

} // namespace tomviz