/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLightingPresetStore_h
#define tomvizLightingPresetStore_h

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace tomviz {
namespace pipeline {

/// One user-saved bundle of volume lighting settings.
struct UserLightingPreset
{
  QString name;
  bool shade = true;
  double ambient = 0.1;
  double diffuse = 0.9;
  double specular = 0.3;
  double specularPower = 30.0;
  double scattering = 0.0;
  double reach = 0.0;
  double anisotropy = 0.0;
  bool smoothNormals = false;

  /// True when every lighting value matches within a small tolerance.
  bool matches(const UserLightingPreset& other) const;

  QJsonObject serialize() const;
  static UserLightingPreset deserialize(const QJsonObject& json);
};

/// The user's saved lighting presets, shared by every volume and kept in
/// the application settings so they survive restarts. Names are unique;
/// saving under an existing name replaces that preset.
class LightingPresetStore : public QObject
{
  Q_OBJECT

public:
  static LightingPresetStore& instance();

  QList<UserLightingPreset> presets() const { return m_presets; }
  bool contains(const QString& name) const;
  /// The preset called @a name, or one with an empty name if none.
  UserLightingPreset preset(const QString& name) const;

  void save(const UserLightingPreset& preset);
  void remove(const QString& name);

signals:
  void changed();

private:
  LightingPresetStore();
  Q_DISABLE_COPY(LightingPresetStore)

  void load();
  void store() const;

  QList<UserLightingPreset> m_presets;
};

} // namespace pipeline
} // namespace tomviz

#endif
