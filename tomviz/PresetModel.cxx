/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetModel.h"

#include <QFont>
#include <QJsonObject>

namespace tomviz {

PresetModel::PresetModel(QObject* parent) : QAbstractTableModel(parent)
{
}

int PresetModel::rowCount(const QModelIndex& id) const
{
  return id.isValid() ? 0 : m_colorMaps.count();
}

int PresetModel::columnCount(const QModelIndex& /*parent*/) const
{
  return 1;
}

QVariant PresetModel::data(const QModelIndex& index, int role) const
{
  switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return m_colorMaps.presetName(index.row());

    case Qt::DecorationRole:
      return m_colorMaps.renderPreview(index.row());

    case Qt::TextAlignmentRole:
      return Qt::AlignCenter + Qt::AlignVCenter;

    case Qt::FontRole:
      if (index.row() == 2) {
        QFont boldFont;
        boldFont.setBold(true);
        return boldFont;
      }
  }

  return QVariant();
}

bool PresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (role == Qt::EditRole) {
    if (!index.isValid())
      return false;

    if (value.toString().trimmed().isEmpty())
      return false;

    m_colorMaps.setPresetName(index.row(), value.toString());
    emit dataChanged(index, index);
    saveSettings();

    return true;
  }
  return false;
}

Qt::ItemFlags PresetModel::flags(const QModelIndex &index) const
{
  return Qt::ItemIsEditable | QAbstractTableModel::flags(index);
}

QVariant PresetModel::headerData(int, Qt::Orientation, int) const
{
  return QVariant();
}

void PresetModel::modelChanged()
{
  saveSettings();
  beginResetModel();
  endResetModel();
}

void PresetModel::setRow(const QModelIndex& index)
{
  m_row = index.row();
}

void PresetModel::updateRow()
{
  m_row = m_colorMaps.count() - 1;
}

QString PresetModel::presetName()
{
  return m_colorMaps.presetName(m_row);
}

void PresetModel::changePreset(const QModelIndex& index)
{
  emit setRow(index);
  emit applyPreset();
}

void PresetModel::addNewPreset(const QJsonObject& newPreset)
{
  m_colorMaps.addPreset(newPreset);
  updateRow();
  modelChanged();
}

void PresetModel::resetToDefaults()
{
  m_colorMaps.resetToDefaults();

  if (m_row >= m_colorMaps.count()) {
    updateRow();
  }

  modelChanged();
}

void PresetModel::saveSettings()
{
  m_colorMaps.save();
}

void PresetModel::deletePreset(const QModelIndex& index)
{
  m_colorMaps.deletePreset(index.row());
  if (m_row >= m_colorMaps.count()) {
    updateRow();
  }

  modelChanged();
}
} // namespace tomviz
