/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSelectVolumeRangeWidget_h
#define tomvizSelectVolumeRangeWidget_h

#include "CustomPythonNodeWidget.h"
#include "PortData.h"

#include <QMap>
#include <QPointer>

class QDoubleSpinBox;
class QVBoxLayout;

namespace tomviz {

class SelectVolumeWidget;

/// Box selection for operators that act on a sub-volume, such as Clear
/// Subvolume: a draggable box in the 3D view plus start/end spin boxes,
/// reported as [start, end) index ranges, and a fill value.
class SelectVolumeRangeWidget : public pipeline::CustomPythonNodeWidget
{
  Q_OBJECT

public:
  SelectVolumeRangeWidget(const QMap<QString, pipeline::PortData>& inputs,
                          QWidget* parent = nullptr);
  ~SelectVolumeRangeWidget() override;

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

private:
  // The box widget takes its selection at construction, so restoring a
  // saved range means rebuilding it
  void buildSelector(const int selection[6]);

  double m_origin[3] = { 0, 0, 0 };
  double m_spacing[3] = { 1, 1, 1 };
  double m_position[3] = { 0, 0, 0 };
  int m_extent[6] = { 0, 0, 0, 0, 0, 0 };
  QVBoxLayout* m_layout = nullptr;
  QPointer<SelectVolumeWidget> m_selector;
  QDoubleSpinBox* m_fillValue = nullptr;
};

} // namespace tomviz

#endif
