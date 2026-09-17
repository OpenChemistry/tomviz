/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LabelSelectionWidget.h"

#include "LabelSelection.h"
#include "pipeline/data/LabelMapData.h"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace tomviz {

namespace {

enum Column
{
  RemoveColumn = 0,
  ColorColumn,
  ValueColumn,
  NameColumn,
  CountColumn,
  ColumnCount
};

/// The label table of the operator's input: the port's own when it is
/// typed as a label map, otherwise a scan of the volume's values.
pipeline::LabelMapDataPtr inputLabels(
  const QMap<QString, pipeline::PortData>& inputs)
{
  auto it = inputs.constFind(QStringLiteral("volume"));
  if (it == inputs.constEnd()) {
    return nullptr;
  }
  pipeline::VolumeDataPtr volume;
  try {
    volume = it.value().value<pipeline::VolumeDataPtr>();
  } catch (...) {
    return nullptr;
  }
  if (!volume || !volume->isValid()) {
    return nullptr;
  }
  auto labels = pipeline::labelMapData(volume);
  if (!labels) {
    labels = std::make_shared<pipeline::LabelMapData>(
      vtkSmartPointer<vtkImageData>(volume->imageData()));
  }
  labels->refreshLabels();
  return labels;
}

} // namespace

LabelSelectionWidget::LabelSelectionWidget(
  const QMap<QString, pipeline::PortData>& inputs, QWidget* parent)
  : CustomPythonNodeWidget(parent)
{
  // wireParameterBindings finds this control by the parameter's name
  setObjectName(QStringLiteral("labels"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(new QLabel(tr("<b>Labels to Remove</b>"), this));

  m_table = new QTableWidget(0, ColumnCount, this);
  m_table->setHorizontalHeaderLabels(
    { QString(), tr("Color"), tr("Value"), tr("Name"), tr("Voxels") });
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->verticalHeader()->setVisible(false);
  auto* header = m_table->horizontalHeader();
  header->setStretchLastSection(false);
  header->setSectionResizeMode(RemoveColumn, QHeaderView::ResizeToContents);
  header->setSectionResizeMode(ColorColumn, QHeaderView::Fixed);
  header->resizeSection(ColorColumn, 48);
  header->setSectionResizeMode(ValueColumn, QHeaderView::ResizeToContents);
  header->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
  header->setSectionResizeMode(CountColumn, QHeaderView::ResizeToContents);
  m_table->setMinimumHeight(180);
  layout->addWidget(m_table);

  auto labels = inputLabels(inputs);
  if (labels && labels->labelsSupported()) {
    for (const auto& entry : labels->labels().entries()) {
      // The background is what the others are removed into
      if (entry.value == 0.0) {
        continue;
      }
      int row = m_table->rowCount();
      m_table->insertRow(row);
      auto* remove = new QTableWidgetItem;
      remove->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                       Qt::ItemIsUserCheckable);
      remove->setCheckState(Qt::Unchecked);
      remove->setData(Qt::UserRole, entry.value);
      m_table->setItem(row, RemoveColumn, remove);
      auto* color = new QTableWidgetItem;
      color->setBackground(entry.color);
      m_table->setItem(row, ColorColumn, color);
      auto* value = new QTableWidgetItem(QString::number(entry.value));
      value->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      m_table->setItem(row, ValueColumn, value);
      m_table->setItem(row, NameColumn,
                       new QTableWidgetItem(entry.displayName()));
      auto* count = new QTableWidgetItem(
        QLocale().toString(qlonglong(entry.voxelCount)));
      count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      m_table->setItem(row, CountColumn, count);
    }
  }

  auto* buttons = new QHBoxLayout;
  auto* all = new QPushButton(tr("All"), this);
  auto* none = new QPushButton(tr("None"), this);
  auto* invert = new QPushButton(tr("Invert"), this);
  buttons->addWidget(all);
  buttons->addWidget(none);
  buttons->addWidget(invert);
  buttons->addStretch();
  layout->addLayout(buttons);
  connect(all, &QPushButton::clicked, this, [this]() { setAllChecked(true); });
  connect(none, &QPushButton::clicked, this,
          [this]() { setAllChecked(false); });
  connect(invert, &QPushButton::clicked, this,
          &LabelSelectionWidget::invertChecked);

  m_summary = new QLabel(this);
  m_summary->setWordWrap(true);
  layout->addWidget(m_summary);

  m_renumber = new QCheckBox(tr("Renumber remaining labels"), this);
  m_renumber->setToolTip(
    tr("Number the labels that remain 1, 2, 3... in ascending order of "
       "their old values. Off, they keep their values so names and colors "
       "in a Label Map visualization still line up."));
  layout->addWidget(m_renumber);

  connect(m_table, &QTableWidget::itemChanged, this,
          [this](QTableWidgetItem* item) {
            if (item && item->column() == RemoveColumn) {
              updateSummary();
              emit selectionChanged();
            }
          });

  if (m_table->rowCount() == 0) {
    m_summary->setText(labels && !labels->labelsSupported()
                         ? tr("The active scalars are floating point, so "
                              "they have no labels to list.")
                         : tr("The input holds no labels yet."));
  } else {
    updateSummary();
  }
}

QVector<double> LabelSelectionWidget::selectedLabels() const
{
  QVector<double> labels = m_unlisted;
  for (int row = 0; row < m_table->rowCount(); ++row) {
    auto* item = m_table->item(row, RemoveColumn);
    if (item && item->checkState() == Qt::Checked) {
      labels.append(item->data(Qt::UserRole).toDouble());
    }
  }
  std::sort(labels.begin(), labels.end());
  return labels;
}

void LabelSelectionWidget::setSelectedLabels(const QVector<double>& labels)
{
  QSignalBlocker blocker(m_table);
  QVector<double> listed;
  for (int row = 0; row < m_table->rowCount(); ++row) {
    auto* item = m_table->item(row, RemoveColumn);
    if (!item) {
      continue;
    }
    double value = item->data(Qt::UserRole).toDouble();
    listed.append(value);
    item->setCheckState(labels.contains(value) ? Qt::Checked : Qt::Unchecked);
  }
  m_unlisted.clear();
  for (double value : labels) {
    if (!listed.contains(value)) {
      m_unlisted.append(value);
    }
  }
  updateSummary();
}

void LabelSelectionWidget::setAllChecked(bool checked)
{
  {
    QSignalBlocker blocker(m_table);
    for (int row = 0; row < m_table->rowCount(); ++row) {
      if (auto* item = m_table->item(row, RemoveColumn)) {
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
      }
    }
  }
  updateSummary();
  emit selectionChanged();
}

void LabelSelectionWidget::invertChecked()
{
  {
    QSignalBlocker blocker(m_table);
    for (int row = 0; row < m_table->rowCount(); ++row) {
      if (auto* item = m_table->item(row, RemoveColumn)) {
        item->setCheckState(item->checkState() == Qt::Checked
                              ? Qt::Unchecked
                              : Qt::Checked);
      }
    }
  }
  updateSummary();
  emit selectionChanged();
}

void LabelSelectionWidget::updateSummary()
{
  if (m_table->rowCount() == 0) {
    return;
  }
  int checked = 0;
  for (int row = 0; row < m_table->rowCount(); ++row) {
    auto* item = m_table->item(row, RemoveColumn);
    if (item && item->checkState() == Qt::Checked) {
      ++checked;
    }
  }
  m_summary->setText(tr("Removing %1 of %2 labels")
                       .arg(checked)
                       .arg(m_table->rowCount()));
}

void LabelSelectionWidget::getValues(QMap<QString, QVariant>& map)
{
  map["labels"] = formatLabelList(selectedLabels());
  map["renumber"] = m_renumber->isChecked();
}

void LabelSelectionWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("labels")) {
    setSelectedLabels(parseLabelList(map["labels"].toString()));
  }
  if (map.contains("renumber")) {
    m_renumber->setChecked(map["renumber"].toBool());
  }
}

} // namespace tomviz
