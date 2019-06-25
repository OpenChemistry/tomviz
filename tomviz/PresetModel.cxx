/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetModel.h"

#include <pqPresetToPixmap.h>

#include <QPair>
#include <QPixmap>
#include <QSize>

namespace tomviz {

PresetModel::PresetModel(QObject* parent) : QAbstractTableModel(parent)
{
  vtkNew<vtkSMTransferFunctionPresets> Presets;
  auto begin = Presets->GetNumberOfPresets();
  QString file = getMatplotlibColorMapFile();
  Presets->ImportPresets(file.toStdString().c_str());

  pqPresetToPixmap PixMapRenderer;
  for (auto i = begin; i < Presets->GetNumberOfPresets(); ++i) {
    m_Pixmaps.push_back(QPair<QString, QPixmap>(
      static_cast<QString>(Presets->GetPresetName(i)),
      PixMapRenderer.render(Presets->GetPreset(i), QSize(135, 20))));
  }
}

int PresetModel::rowCount(const QModelIndex& id) const
{
  return id.isValid() ? 0 : m_Pixmaps.size();
}

int PresetModel::columnCount(const QModelIndex& /*parent*/) const
{
  return 1;
}

QVariant PresetModel::data(const QModelIndex& index, int role) const
{
  switch (role) {
    case Qt::DisplayRole:
      return m_Pixmaps[index.row()].first;

    case Qt::DecorationRole:
      return m_Pixmaps[index.row()].second;

    case Qt::TextAlignmentRole:
      return Qt::AlignLeft + Qt::AlignVCenter;
  }

  return QVariant();
}

QVariant PresetModel::headerData(int, Qt::Orientation /*orientation*/,
				 int /*role*/) const
{
  return QVariant();
}
  
void PresetModel::setName(const QModelIndex& index)
{
  m_name = m_Pixmaps[index.row()].first;
}

QString PresetModel::presetName()
{
  return m_name;
}

void PresetModel::changePreset(const QModelIndex& index)
{
  emit setName(index);
  emit applyPreset();
}

QString PresetModel::getMatplotlibColorMapFile()
{
  QString path = QApplication::applicationDirPath() +
                 "/../share/tomviz/matplotlib_cmaps.json";
  QFile file(path);
  if (file.exists()) {
    return path;
  }
// On OSX the above doesn't work in a build tree.  It is fine
// for superbuilds, but the following is needed in the build tree
// since the executable is three levels down in bin/tomviz.app/Contents/MacOS/
#ifdef __APPLE__
  else {
    path = QApplication::applicationDirPath() +
           "/../../../../share/tomviz/matplotlib_cmaps.json";
    return path;
  }
#else
  return "";
#endif
}
} // namespace tomviz
