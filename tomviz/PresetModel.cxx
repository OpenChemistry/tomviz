/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetModel.h"

#include <pqPresetToPixmap.h>

#include <QPair>
#include <QPixmap>
#include <QScopedPointer>
#include <QSize>

namespace tomviz {

// ctor
PresetModel::PresetModel(QObject *parent)
  : QAbstractTableModel(parent)
{
  //  vtkNew<vtkSMTransferFunctionPresets> Presets;
  auto begin = Presets->GetNumberOfPresets();
  QString file = "../tomviz/tomviz/resources/matplotlib_cmaps.json";
  Presets->ImportPresets(file.toStdString().c_str());

  pqPresetToPixmap PixMapRenderer;
  for (auto i = begin; i < Presets->GetNumberOfPresets(); ++i) {
    m_Pixmaps.push_back(
      QPair<QString, QPixmap>(static_cast<QString>(Presets->GetPresetName(i)),
			      PixMapRenderer.render(Presets->GetPreset(i), QSize(135, 20))));
  }
}

// the number of rows the view should display
int PresetModel::rowCount(const QModelIndex &id) const
{
  return id.isValid() ? 0 : m_Pixmaps.size();
}

// the number of columns the view should display
int PresetModel::columnCount(const QModelIndex &/*parent*/) const
{
  return 1;
}

// returns what value should be in the cell at a given index
QVariant PresetModel::data(const QModelIndex &index, int role) const
{ 
  switch (role) {
    case Qt::DisplayRole :
      return m_Pixmaps[index.row()].first;

    case Qt::DecorationRole:
      return m_Pixmaps[index.row()].second;

    case Qt::TextAlignmentRole:
      return Qt::AlignLeft + Qt::AlignVCenter;
  }
 
  return QVariant();
}

QVariant PresetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role ==Qt::DisplayRole && orientation == Qt::Horizontal) {
    return QString("Presets");
  }

  return QVariant();
}

void PresetModel::handleClick(const QModelIndex &index)
{
  std::cout << "The Preset "
	    << m_Pixmaps[index.row()].first.toStdString()
	    << " was selected." << std::endl;
  const Json::Value& preset = Presets->GetPreset(index.row()+232);
  emit this->getPreset(preset);
}
} // namespace tomviz
