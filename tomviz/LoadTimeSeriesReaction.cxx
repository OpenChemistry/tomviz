/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LoadTimeSeriesReaction.h"

#include "LoadDataReaction.h"

namespace tomviz {

LoadTimeSeriesReaction::LoadTimeSeriesReaction(QAction* parentObject)
  : pqReaction(parentObject)
{}

LoadTimeSeriesReaction::~LoadTimeSeriesReaction() = default;

void LoadTimeSeriesReaction::onTriggered()
{
  loadData();
}

QList<DataSource*> LoadTimeSeriesReaction::loadData()
{
  bool isTimeSeries = true;
  return LoadDataReaction::loadData(isTimeSeries);
}

} // end of namespace tomviz
