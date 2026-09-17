/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ThresholdRangeWidget.h"

#include "DoubleSliderWidget.h"
#include "pipeline/data/VolumeData.h"

#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>

namespace tomviz {

ThresholdRangeWidget::ThresholdRangeWidget(
  const QMap<QString, pipeline::PortData>& inputs, QWidget* parent)
  : CustomPythonNodeWidget(parent)
{
  double lowerGuess = 0.0;
  if (auto it = inputs.constFind(QStringLiteral("volume"));
      it != inputs.constEnd()) {
    if (auto vol = it.value().value<pipeline::VolumeDataPtr>();
        vol && vol->isValid()) {
      auto range = vol->scalarRange();
      m_dataRange[0] = range[0];
      m_dataRange[1] = range[1];
      lowerGuess = vol->thresholdSeed();
    }
  }
  if (!(m_dataRange[1] > m_dataRange[0])) {
    m_dataRange[1] = m_dataRange[0] + 1.0;
  }

  auto* layout = new QFormLayout(this);
  auto makeSlider = [this]() {
    auto* slider = new DoubleSliderWidget(true, this);
    slider->setLineEditWidth(70);
    slider->setMinimum(m_dataRange[0]);
    slider->setMaximum(m_dataRange[1]);
    slider->setResolution(1000);
    return slider;
  };
  m_lower = makeSlider();
  m_upper = makeSlider();
  // wireParameterBindings finds the controls by parameter name
  m_lower->setObjectName("lower_threshold");
  m_upper->setObjectName("upper_threshold");
  layout->addRow("Lower Threshold", m_lower);
  layout->addRow("Upper Threshold", m_upper);
  layout->addRow(new QLabel(QString("Data range: %1 to %2")
                              .arg(m_dataRange[0])
                              .arg(m_dataRange[1]),
                            this));

  // Keep lower <= upper
  connect(m_lower, &DoubleSliderWidget::valueEdited, this, [this](double v) {
    if (v > m_upper->value()) {
      m_upper->setValue(v);
    }
  });
  connect(m_upper, &DoubleSliderWidget::valueEdited, this, [this](double v) {
    if (v < m_lower->value()) {
      m_lower->setValue(v);
    }
  });

  // Until told otherwise, mirror ThresholdSink's initial pick: the
  // brightest voxels of the data
  setThresholds(lowerGuess, m_dataRange[1]);
}

void ThresholdRangeWidget::setThresholds(double lower, double upper)
{
  m_lower->setValue(lower);
  m_upper->setValue(upper);
}

void ThresholdRangeWidget::getValues(QMap<QString, QVariant>& map)
{
  map["lower_threshold"] = m_lower->value();
  map["upper_threshold"] = m_upper->value();
}

void ThresholdRangeWidget::setValues(const QMap<QString, QVariant>& map)
{
  // Values still equal to the JSON defaults mean the node has never been
  // edited, so keep the data-based guess rather than the meaningless
  // declared numbers. A bound Threshold visualization overrides both.
  auto isDefault = [this, &map](const QString& key) {
    return !map.contains(key) ||
           (m_jsonDefaults.contains(key) &&
            qFuzzyCompare(1.0 + map[key].toDouble(),
                          1.0 + m_jsonDefaults[key]));
  };
  if (isDefault("lower_threshold") && isDefault("upper_threshold")) {
    return;
  }
  setThresholds(map.value("lower_threshold", m_lower->value()).toDouble(),
                map.value("upper_threshold", m_upper->value()).toDouble());
}

void ThresholdRangeWidget::setJSONDescription(const QString& json)
{
  CustomPythonNodeWidget::setJSONDescription(json);
  m_jsonDefaults.clear();
  auto doc = QJsonDocument::fromJson(json.toUtf8());
  for (const auto& value : doc.object()["parameters"].toArray()) {
    auto param = value.toObject();
    if (param["default"].isDouble()) {
      m_jsonDefaults[param["name"].toString()] = param["default"].toDouble();
    }
  }
}

} // namespace tomviz
