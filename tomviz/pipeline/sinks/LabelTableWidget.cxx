/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LabelTableWidget.h"

#include "LabelMapSink.h"
#include "data/LabelMapData.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

namespace tomviz {
namespace pipeline {

LabelTableModel::LabelTableModel(QObject* parent)
  : QAbstractTableModel(parent)
{}

std::shared_ptr<LabelMapData> LabelTableModel::lock() const
{
  return m_labelMap.lock();
}

void LabelTableModel::setLabelMap(
  const std::shared_ptr<LabelMapData>& labelMap)
{
  beginResetModel();
  m_labelMap = labelMap;
  endResetModel();
}

int LabelTableModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  auto labelMap = lock();
  return labelMap ? labelMap->labels().count() : 0;
}

int LabelTableModel::columnCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : ColumnCount;
}

QVariant LabelTableModel::data(const QModelIndex& index, int role) const
{
  auto labelMap = lock();
  if (!labelMap || !index.isValid() ||
      index.row() >= labelMap->labels().count()) {
    return QVariant();
  }
  const auto& entry = labelMap->labels().at(index.row());

  if (role == Qt::CheckStateRole && index.column() == VisibleColumn) {
    return entry.visible ? Qt::Checked : Qt::Unchecked;
  }
  if (role == Qt::DecorationRole && index.column() == ColorColumn) {
    return entry.color;
  }
  if (role == Qt::DisplayRole || role == Qt::EditRole) {
    switch (index.column()) {
      case ValueColumn:
        return entry.value;
      case NameColumn:
        // EditRole hands back the stored name so that starting an edit
        // does not turn the "Label 3" placeholder into a real name.
        return role == Qt::EditRole ? entry.name : entry.displayName();
      case CountColumn:
        return QLocale().toString(qlonglong(entry.voxelCount));
      default:
        return QVariant();
    }
  }
  // Sorting runs through the proxy on this role so that Value and
  // Voxels order numerically rather than by their formatted strings.
  if (role == Qt::UserRole) {
    switch (index.column()) {
      case VisibleColumn:
        return entry.visible;
      case ValueColumn:
        return entry.value;
      case NameColumn:
        return entry.displayName();
      case CountColumn:
        return qlonglong(entry.voxelCount);
      default:
        return QVariant();
    }
  }
  if (role == Qt::TextAlignmentRole &&
      (index.column() == ValueColumn || index.column() == CountColumn)) {
    return int(Qt::AlignRight | Qt::AlignVCenter);
  }
  return QVariant();
}

bool LabelTableModel::setData(const QModelIndex& index,
                              const QVariant& value, int role)
{
  auto labelMap = lock();
  if (!labelMap || !index.isValid() ||
      index.row() >= labelMap->labels().count()) {
    return false;
  }

  if (role == Qt::CheckStateRole && index.column() == VisibleColumn) {
    labelMap->labels().setVisible(
      index.row(), value.toInt() == Qt::Checked);
  } else if (role == Qt::EditRole && index.column() == NameColumn) {
    labelMap->labels().setName(index.row(), value.toString());
  } else {
    return false;
  }

  emit dataChanged(index, index);
  emit labelsEdited();
  return true;
}

QVariant LabelTableModel::headerData(int section,
                                     Qt::Orientation orientation,
                                     int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return QVariant();
  }
  switch (section) {
    case VisibleColumn:
      return QString();
    case ColorColumn:
      return tr("Color");
    case ValueColumn:
      return tr("Value");
    case NameColumn:
      return tr("Name");
    case CountColumn:
      return tr("Voxels");
    default:
      return QVariant();
  }
}

Qt::ItemFlags LabelTableModel::flags(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  auto base = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  switch (index.column()) {
    case VisibleColumn:
      return base | Qt::ItemIsUserCheckable;
    case NameColumn:
      return base | Qt::ItemIsEditable;
    default:
      return base;
  }
}

void LabelTableModel::setAllVisible(bool visible)
{
  auto labelMap = lock();
  if (!labelMap || labelMap->labels().isEmpty()) {
    return;
  }
  labelMap->labels().setAllVisible(visible);
  emit dataChanged(index(0, VisibleColumn),
                   index(labelMap->labels().count() - 1, VisibleColumn));
  emit labelsEdited();
}

void LabelTableModel::invertVisibility()
{
  auto labelMap = lock();
  if (!labelMap || labelMap->labels().isEmpty()) {
    return;
  }
  auto& table = labelMap->labels();
  for (int i = 0; i < table.count(); ++i) {
    table.setVisible(i, !table.at(i).visible);
  }
  emit dataChanged(index(0, VisibleColumn),
                   index(table.count() - 1, VisibleColumn));
  emit labelsEdited();
}

void LabelTableModel::setColor(int row, const QColor& color)
{
  auto labelMap = lock();
  if (!labelMap || row < 0 || row >= labelMap->labels().count()) {
    return;
  }
  labelMap->labels().setColor(row, color);
  emit dataChanged(index(row, ColorColumn), index(row, ColorColumn));
  emit labelsEdited();
}

void LabelTableModel::refreshVisibility()
{
  auto labelMap = lock();
  if (!labelMap || labelMap->labels().isEmpty()) {
    return;
  }
  emit dataChanged(index(0, VisibleColumn),
                   index(labelMap->labels().count() - 1, VisibleColumn),
                   { Qt::CheckStateRole });
}

LabelTableWidget::LabelTableWidget(LabelMapSink* sink, QWidget* parent)
  : QWidget(parent), m_sink(sink)
{
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(new QLabel(tr("<b>Labels</b>"), this));

  m_model = new LabelTableModel(this);
  m_proxy = new QSortFilterProxyModel(this);
  m_proxy->setSourceModel(m_model);
  m_proxy->setSortRole(Qt::UserRole);
  m_proxy->setFilterKeyColumn(LabelTableModel::NameColumn);
  m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

  m_filter = new QLineEdit(this);
  m_filter->setPlaceholderText(tr("Filter labels"));
  m_filter->setClearButtonEnabled(true);
  layout->addWidget(m_filter);
  connect(m_filter, &QLineEdit::textChanged, m_proxy,
          &QSortFilterProxyModel::setFilterFixedString);

  m_view = new QTableView(this);
  m_view->setModel(m_proxy);
  m_view->setSortingEnabled(true);
  m_view->sortByColumn(LabelTableModel::ValueColumn, Qt::AscendingOrder);
  m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_view->verticalHeader()->setVisible(false);

  // Name is the only column worth giving spare width to; the rest are a
  // checkbox, a swatch and two short numbers. Without this the name is
  // the column that gets truncated and the voxel count scrolls off.
  auto* header = m_view->horizontalHeader();
  header->setStretchLastSection(false);
  header->setSectionResizeMode(LabelTableModel::VisibleColumn,
                               QHeaderView::ResizeToContents);
  header->setSectionResizeMode(LabelTableModel::ColorColumn,
                               QHeaderView::Fixed);
  header->resizeSection(LabelTableModel::ColorColumn, 48);
  header->setSectionResizeMode(LabelTableModel::ValueColumn,
                               QHeaderView::ResizeToContents);
  header->setSectionResizeMode(LabelTableModel::NameColumn,
                               QHeaderView::Stretch);
  header->setSectionResizeMode(LabelTableModel::CountColumn,
                               QHeaderView::ResizeToContents);
  m_view->setMinimumHeight(180);
  layout->addWidget(m_view);
  connect(m_view, &QTableView::doubleClicked, this,
          &LabelTableWidget::onDoubleClicked);

  auto* buttons = new QHBoxLayout;
  auto* showAll = new QPushButton(tr("Show All"), this);
  auto* hideAll = new QPushButton(tr("Hide All"), this);
  auto* invert = new QPushButton(tr("Invert"), this);
  buttons->addWidget(showAll);
  buttons->addWidget(hideAll);
  buttons->addWidget(invert);
  buttons->addStretch();
  layout->addLayout(buttons);

  connect(showAll, &QPushButton::clicked, this,
          [this]() { m_model->setAllVisible(true); });
  connect(hideAll, &QPushButton::clicked, this,
          [this]() { m_model->setAllVisible(false); });
  connect(invert, &QPushButton::clicked, this,
          [this]() { m_model->invertVisibility(); });

  m_summary = new QLabel(this);
  m_summary->setWordWrap(true);
  layout->addWidget(m_summary);

  connect(m_model, &LabelTableModel::labelsEdited, this,
          &LabelTableWidget::applyEdits);
  if (m_sink) {
    connect(m_sink, &LabelMapSink::labelsChanged, this,
            &LabelTableWidget::refresh);
    // A Remove Labels editor bound to this sink can flip visibility
    // from outside the panel
    connect(m_sink, &LabelMapSink::labelVisibilityChanged, m_model,
            &LabelTableModel::refreshVisibility);
  }

  refresh();
}

void LabelTableWidget::refresh()
{
  auto labelMap = m_sink ? m_sink->labelMap() : nullptr;
  m_model->setLabelMap(labelMap);

  if (!labelMap) {
    m_summary->setText(tr("No label map data yet."));
    return;
  }
  if (!labelMap->labelsSupported()) {
    m_summary->setText(
      tr("The active scalars are floating point, so they have no "
         "distinct labels to list. Convert them to an integer type "
         "first."));
    return;
  }
  const auto& table = labelMap->labels();
  if (table.isTruncated()) {
    m_summary->setText(tr("Showing the first %1 labels; the data holds "
                          "more than this panel can list.")
                         .arg(table.count()));
  } else {
    m_summary->setText(table.count() == 1
                         ? tr("1 label")
                         : tr("%1 labels").arg(table.count()));
  }
}

void LabelTableWidget::onDoubleClicked(const QModelIndex& proxyIndex)
{
  if (proxyIndex.column() != LabelTableModel::ColorColumn) {
    return;
  }
  auto sourceIndex = m_proxy->mapToSource(proxyIndex);
  auto current =
    m_model->data(sourceIndex, Qt::DecorationRole).value<QColor>();
  auto chosen = QColorDialog::getColor(current, this, tr("Label Color"));
  if (chosen.isValid()) {
    m_model->setColor(sourceIndex.row(), chosen);
  }
}

void LabelTableWidget::applyEdits()
{
  if (m_sink) {
    m_sink->labelTableEdited();
  }
}

} // namespace pipeline
} // namespace tomviz
