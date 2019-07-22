/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizColorMap_h
#define tomvizColorMap_h

#include <QObject>

#include <QJsonArray>
#include <QPixmap>

class vtkSMProxy;

namespace tomviz {

/**
 * Keep track of the loaded color maps, the current default, setting colors.
 */
class ColorMap : public QObject
{
  Q_OBJECT

public:
  /**
   * Returns a reference to the singleton instance of the class.
   */
  static ColorMap& instance();

  /**
   * Default preset name.
   */
  QString defaultPresetName() const;

  /**
   * Return the name of the preset for the supplied index, or "Error".
   */
  QString presetName(int index) const;

  /**
   * Set the name of the preset for the supplied index.
   */
  void setPresetName(int index, const QString& name);

  /**
   * Restore all default color maps from default color map file.
   */
  void resetToDefaults();

  /**
   * Add a new preset.
   */
  int addPreset(const QJsonObject& preset);

  /**
   * Delete the specified preset.
   */
  bool deletePreset(int index);

  /**
   * Get the current number of color map presets.
   */
  int count() const;

  /**
   * Render a preview of the color map into an image.
   */
  QPixmap renderPreview(int index) const;

  /**
   * Save the current presets as application defaults.
   */
  void save();

  /**
   * Apply the default preset to the supplied transfer function proxy.
   */
  void applyPreset(vtkSMProxy* proxy) const;

  /**
   * Apply the specified color map to the supplied transfer function proxy.
   */
  void applyPreset(int index, vtkSMProxy* proxy) const;
  void applyPreset(const QString& name, vtkSMProxy* proxy) const;

protected:
  ColorMap();
  ~ColorMap() override;
  void loadFromFile();

  QJsonArray m_presets;
  QString m_defaultName = "Plasma";

private:
  Q_DISABLE_COPY(ColorMap)
};

} // namespace tomviz

#endif
