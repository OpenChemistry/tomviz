/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizThresholdRangeWidget_h
#define tomvizThresholdRangeWidget_h

#include "CustomPythonNodeWidget.h"
#include "PortData.h"

#include <QMap>

namespace tomviz {

class DoubleSliderWidget;

/// Lower/upper threshold sliders that span the input's actual scalar
/// range, for the Binary Threshold operator. The sliders are named after
/// their parameters so the operator's "bindToSink" declarations can link
/// them live to a Threshold visualization on the same data.
class ThresholdRangeWidget : public pipeline::CustomPythonNodeWidget
{
  Q_OBJECT

public:
  ThresholdRangeWidget(const QMap<QString, pipeline::PortData>& inputs,
                       QWidget* parent = nullptr);

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;
  void setJSONDescription(const QString& json) override;

private:
  void setThresholds(double lower, double upper);

  DoubleSliderWidget* m_lower = nullptr;
  DoubleSliderWidget* m_upper = nullptr;
  double m_dataRange[2] = { 0.0, 1.0 };
  // Defaults declared in the JSON, to recognise a never-edited node
  QMap<QString, double> m_jsonDefaults;
};

} // namespace tomviz

#endif
