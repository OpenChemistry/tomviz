/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LightingPresetStore.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>

namespace tomviz {
namespace pipeline {

namespace {
const char* kSettingsKey = "Volume.UserLightingPresets";
}

bool UserLightingPreset::matches(const UserLightingPreset& o) const
{
  auto approx = [](double a, double b) { return std::fabs(a - b) < 1e-3; };
  return shade == o.shade && approx(ambient, o.ambient) &&
         approx(diffuse, o.diffuse) && approx(specular, o.specular) &&
         approx(specularPower, o.specularPower) &&
         approx(scattering, o.scattering) && approx(reach, o.reach) &&
         approx(anisotropy, o.anisotropy) &&
         smoothNormals == o.smoothNormals;
}

QJsonObject UserLightingPreset::serialize() const
{
  QJsonObject json;
  json["name"] = name;
  json["shade"] = shade;
  json["ambient"] = ambient;
  json["diffuse"] = diffuse;
  json["specular"] = specular;
  json["specularPower"] = specularPower;
  json["scattering"] = scattering;
  json["reach"] = reach;
  json["anisotropy"] = anisotropy;
  json["smoothNormals"] = smoothNormals;
  return json;
}

UserLightingPreset UserLightingPreset::deserialize(const QJsonObject& json)
{
  UserLightingPreset p;
  p.name = json["name"].toString();
  p.shade = json["shade"].toBool(p.shade);
  p.ambient = json["ambient"].toDouble(p.ambient);
  p.diffuse = json["diffuse"].toDouble(p.diffuse);
  p.specular = json["specular"].toDouble(p.specular);
  p.specularPower = json["specularPower"].toDouble(p.specularPower);
  p.scattering = json["scattering"].toDouble(p.scattering);
  p.reach = json["reach"].toDouble(p.reach);
  p.anisotropy = json["anisotropy"].toDouble(p.anisotropy);
  p.smoothNormals = json["smoothNormals"].toBool(p.smoothNormals);
  return p;
}

LightingPresetStore& LightingPresetStore::instance()
{
  static LightingPresetStore store;
  return store;
}

LightingPresetStore::LightingPresetStore()
{
  load();
}

bool LightingPresetStore::contains(const QString& name) const
{
  for (const auto& p : m_presets) {
    if (p.name == name) {
      return true;
    }
  }
  return false;
}

UserLightingPreset LightingPresetStore::preset(const QString& name) const
{
  for (const auto& p : m_presets) {
    if (p.name == name) {
      return p;
    }
  }
  return UserLightingPreset{};
}

void LightingPresetStore::save(const UserLightingPreset& preset)
{
  if (preset.name.trimmed().isEmpty()) {
    return;
  }
  bool replaced = false;
  for (auto& existing : m_presets) {
    if (existing.name == preset.name) {
      existing = preset;
      replaced = true;
    }
  }
  if (!replaced) {
    m_presets.append(preset);
  }
  store();
  emit changed();
}

void LightingPresetStore::remove(const QString& name)
{
  for (int i = 0; i < m_presets.size(); ++i) {
    if (m_presets[i].name == name) {
      m_presets.removeAt(i);
      store();
      emit changed();
      return;
    }
  }
}

bool LightingPresetStore::rename(const QString& name, const QString& newName)
{
  const QString trimmed = newName.trimmed();
  if (trimmed.isEmpty() || (trimmed != name && contains(trimmed))) {
    return false;
  }
  for (auto& preset : m_presets) {
    if (preset.name == name) {
      if (preset.name != trimmed) {
        preset.name = trimmed;
        store();
        emit changed();
      }
      return true;
    }
  }
  return false;
}

void LightingPresetStore::load()
{
  // Headless (tests) has no application core: the store is memory-only
  auto* core = pqApplicationCore::instance();
  auto* settings = core ? core->settings() : nullptr;
  if (!settings) {
    return;
  }
  auto doc = QJsonDocument::fromJson(
    settings->value(kSettingsKey, QString()).toString().toUtf8());
  m_presets.clear();
  for (const auto& value : doc.array()) {
    auto p = UserLightingPreset::deserialize(value.toObject());
    if (!p.name.isEmpty()) {
      m_presets.append(p);
    }
  }
}

void LightingPresetStore::store() const
{
  auto* core = pqApplicationCore::instance();
  auto* settings = core ? core->settings() : nullptr;
  if (!settings) {
    return;
  }
  QJsonArray array;
  for (const auto& p : m_presets) {
    array.append(p.serialize());
  }
  settings->setValue(kSettingsKey,
                     QString::fromUtf8(QJsonDocument(array).toJson(
                       QJsonDocument::Compact)));
}

} // namespace pipeline
} // namespace tomviz
