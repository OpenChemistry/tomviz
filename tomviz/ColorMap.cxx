/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ColorMap.h"

#include <pqApplicationCore.h>
#include <pqPresetToPixmap.h>
#include <pqSettings.h>
#include <vtkDataArray.h>
#include <vtkSMTransferFunctionProxy.h>

#include <vtk_jsoncpp.h>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <set>

namespace tomviz
{

QColor segmentationLabelColor(int index)
{
  static const double goldenAngle = 137.508;
  double hue = std::fmod(index * goldenAngle, 360.0) / 360.0;
  double sat = 0.65 + 0.35 * ((index % 3) / 2.0);
  double brightness = 0.75 + 0.25 * ((index + 1) % 2);

  double c = brightness * sat;
  double x = c * (1.0 - std::abs(std::fmod(hue * 6.0, 2.0) - 1.0));
  double m = brightness - c;

  double r, g, b;
  double h6 = hue * 6.0;
  if (h6 < 1.0) {
    r = c; g = x; b = 0;
  } else if (h6 < 2.0) {
    r = x; g = c; b = 0;
  } else if (h6 < 3.0) {
    r = 0; g = c; b = x;
  } else if (h6 < 4.0) {
    r = 0; g = x; b = c;
  } else if (h6 < 5.0) {
    r = x; g = 0; b = c;
  } else {
    r = c; g = 0; b = x;
  }

  return QColor::fromRgbF(r + m, g + m, b + m);
}

QJsonObject buildSegmentationPreset(vtkDataArray* scalars)
{
  if (!scalars || scalars->GetNumberOfTuples() == 0) {
    return QJsonObject();
  }
  const int dataType = scalars->GetDataType();
  if (dataType == VTK_FLOAT || dataType == VTK_DOUBLE) {
    return QJsonObject();
  }

  std::set<double> uniqueValues;
  const vtkIdType numTuples = scalars->GetNumberOfTuples();
  for (vtkIdType i = 0; i < numTuples; ++i) {
    uniqueValues.insert(scalars->GetTuple1(i));
  }
  if (uniqueValues.empty()) {
    return QJsonObject();
  }

  QJsonArray colors;
  int idx = 0;
  const int lastIdx = static_cast<int>(uniqueValues.size()) - 1;
  for (double val : uniqueValues) {
    // By value, as LabelTable::reconcile colors, so a label keeps its
    // color whichever path built the map
    QColor color = segmentationLabelColor(static_cast<int>(std::lround(val)));

    // Two nodes per label at val -/+ 0.25 with the same color give each
    // integer label its own constant-color band under plain RGB
    // interpolation. Do not use the Step color space here: VTK places
    // its step transitions near segment midpoints, not at the nodes, so
    // consecutive labels can land inside one step and share a color.
    // The ramps between bands sit at non-integer values no label
    // occupies, and the 0.5 minimum node gap keeps the GPU lookup
    // texture size estimate small. The outermost edges sit exactly at
    // the data min/max so a rescale to the data range (e.g. the user's
    // "Reset data range") is an identity and cannot shift the bands.
    double lowOffset = (idx == 0) ? 0.0 : -0.25;
    double highOffset = (idx == lastIdx) ? 0.0 : 0.25;
    for (double offset : { lowOffset, highOffset }) {
      colors.append(val + offset);
      colors.append(color.redF());
      colors.append(color.greenF());
      colors.append(color.blueF());
    }
    ++idx;
  }

  return QJsonObject{ { "name", "Segmentation" },
                      { "colorSpace", "RGB" },
                      { "colors", colors } };
}

void applyPresetToProxy(const QJsonObject& preset, vtkSMProxy* proxy,
                        bool rescaleToCurrentRange)
{
  if (!proxy || preset.isEmpty()) {
    return;
  }

  QJsonObject pqPreset(preset);
  pqPreset.insert("RGBPoints", pqPreset["colors"]);
  pqPreset.insert("ColorSpace", pqPreset["colorSpace"]);

  QJsonDocument doc(pqPreset);
  QByteArray json = doc.toJson(QJsonDocument::Compact);
  Json::Value value;
  std::string errors;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  reader->parse(json.data(), json.data() + json.size(), &value, &errors);

  vtkSMTransferFunctionProxy::ApplyPreset(proxy, value,
                                          rescaleToCurrentRange);
}

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
  QByteArray presetLatin1 = preset.toLatin1();

  Json::Value colors;
  std::string errors;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  reader->parse(presetLatin1.data(), presetLatin1.data() + presetLatin1.size(),
               &colors, &errors);

  pqPresetToPixmap PixMapRenderer;
  return PixMapRenderer.render(colors, QSize(135, 20));
}

void ColorMap::save()
{
  QJsonDocument doc(m_presets);
  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("presetColors", doc.toJson(QJsonDocument::Compact));
}

void ColorMap::applyPreset(vtkSMProxy* proxy) const
{
  applyPreset(defaultPresetName(), proxy);
}

void ColorMap::applyPreset(int index, vtkSMProxy* proxy) const
{
  if (index < 0 || index >= m_presets.size()) {
    return;
  }
  applyPresetToProxy(m_presets[index].toObject(), proxy);
}

void ColorMap::applyPreset(const QString& name, vtkSMProxy* proxy) const
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

  if (!file.open(QIODevice::ReadOnly)) {
    qCritical() << "Unable to open color map file:" << file.fileName();
    return;
  }
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
