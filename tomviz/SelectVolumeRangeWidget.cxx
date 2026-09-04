/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SelectVolumeRangeWidget.h"

#include "ActiveObjects.h"
#include "SelectVolumeWidget.h"
#include "pipeline/Node.h"
#include "pipeline/data/VolumeData.h"

#include <vtkImageData.h>

#include <QDoubleSpinBox>
#include <QHideEvent>
#include <QShowEvent>
#include <QFormLayout>
#include <QVBoxLayout>

#include <limits>

namespace tomviz {

SelectVolumeRangeWidget::SelectVolumeRangeWidget(
  const QMap<QString, pipeline::PortData>& inputs, QWidget* parent)
  : CustomPythonNodeWidget(parent)
{
  if (auto it = inputs.constFind(QStringLiteral("volume"));
      it != inputs.constEnd()) {
    if (auto vol = it.value().value<pipeline::VolumeDataPtr>();
        vol && vol->isValid()) {
      auto* image = vol->imageData();
      image->GetOrigin(m_origin);
      image->GetSpacing(m_spacing);
      image->GetExtent(m_extent);
      auto position = vol->displayPosition();
      std::copy(position.begin(), position.end(), m_position);
    }
  }

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  buildSelector(m_extent);

  connect(&ActiveObjects::instance(), &ActiveObjects::activeNodeChanged, this,
          [this](pipeline::Node*) { updateBoxEnabled(); });

  auto* form = new QFormLayout;
  m_fillValue = new QDoubleSpinBox(this);
  m_fillValue->setRange(std::numeric_limits<double>::lowest(),
                        std::numeric_limits<double>::max());
  m_fillValue->setDecimals(3);
  m_fillValue->setValue(0.0);
  form->addRow("Fill Value", m_fillValue);
  m_layout->addLayout(form);
}

SelectVolumeRangeWidget::~SelectVolumeRangeWidget() = default;

void SelectVolumeRangeWidget::buildSelector(const int selection[6])
{
  if (m_selector) {
    m_layout->removeWidget(m_selector);
    delete m_selector;
  }
  m_selector = new SelectVolumeWidget(m_origin, m_spacing, m_extent, selection,
                                      m_position, this);
  m_selector->setBoxEnabled(false); // shown by updateBoxEnabled when due
  m_layout->insertWidget(0, m_selector);
  updateBoxEnabled();
}

void SelectVolumeRangeWidget::updateBoxEnabled()
{
  if (!m_selector) {
    return;
  }
  bool active = !m_node || ActiveObjects::instance().activeNode() == m_node;
  m_selector->setBoxEnabled(isVisible() && active);
}

void SelectVolumeRangeWidget::setNodeContext(pipeline::Node* node,
                                             pipeline::Pipeline*)
{
  m_node = node;
  updateBoxEnabled();
}

void SelectVolumeRangeWidget::showEvent(QShowEvent* event)
{
  CustomPythonNodeWidget::showEvent(event);
  updateBoxEnabled();
}

void SelectVolumeRangeWidget::hideEvent(QHideEvent* event)
{
  CustomPythonNodeWidget::hideEvent(event);
  updateBoxEnabled();
}

void SelectVolumeRangeWidget::getValues(QMap<QString, QVariant>& map)
{
  int selected[6] = { 0, 0, 0, 0, 0, 0 };
  if (m_selector) {
    m_selector->getExtentOfSelection(selected);
  }
  // The box reports inclusive VTK extents; the operator slices the numpy
  // array [start, end), which starts at the extent's low corner
  const char* keys[3] = { "XRANGE", "YRANGE", "ZRANGE" };
  for (int axis = 0; axis < 3; ++axis) {
    int base = m_extent[2 * axis];
    map[keys[axis]] = QVariantList{ selected[2 * axis] - base,
                                    selected[2 * axis + 1] + 1 - base };
  }
  map["fill_value"] = m_fillValue->value();
}

void SelectVolumeRangeWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("fill_value")) {
    m_fillValue->setValue(map["fill_value"].toDouble());
  }

  int selection[6];
  std::copy(m_extent, m_extent + 6, selection);
  bool anyRange = false;
  const char* keys[3] = { "XRANGE", "YRANGE", "ZRANGE" };
  for (int axis = 0; axis < 3; ++axis) {
    auto range = map.value(keys[axis]).toList();
    if (range.size() != 2) {
      continue;
    }
    int base = m_extent[2 * axis];
    int start = range[0].toInt() + base;
    int end = range[1].toInt() - 1 + base;
    if (end < start) {
      continue; // the declared [0, 0] default: nothing chosen yet
    }
    selection[2 * axis] = qBound(m_extent[2 * axis], start, m_extent[2 * axis + 1]);
    selection[2 * axis + 1] =
      qBound(m_extent[2 * axis], end, m_extent[2 * axis + 1]);
    anyRange = true;
  }
  if (!anyRange) {
    return;
  }
  int current[6];
  m_selector->getExtentOfSelection(current);
  if (!std::equal(current, current + 6, selection)) {
    buildSelector(selection);
  }
}

} // namespace tomviz
