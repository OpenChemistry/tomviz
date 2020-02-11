/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SetDataTypeReaction.h"

#include "ActiveObjects.h"
#include "OperatorFactory.h"
#include "Pipeline.h"
#include "SetTiltAnglesReaction.h"

#include <cassert>

namespace tomviz {

SetDataTypeReaction::SetDataTypeReaction(QAction* action, QMainWindow* mw,
                                         DataSource::DataSourceType t)
  : pqReaction(action), m_mainWindow(mw), m_type(t)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  setWidgetText(t);
  updateEnableState();
}

void SetDataTypeReaction::setDataType(QMainWindow* mw, DataSource* dsource,
                                      DataSource::DataSourceType t)
{
  if (dsource == nullptr) {
    dsource = ActiveObjects::instance().activeDataSource();
  }
  // if it is still null, give up
  if (dsource == nullptr) {
    return;
  }
  if (t == DataSource::TiltSeries) {
    SetTiltAnglesReaction::showSetTiltAnglesUI(mw, dsource);
  } else {
    // If it was a TiltSeries convert to volume
    // if (dsource->type() == DataSource::TiltSeries) {
    Operator* op = OperatorFactory::instance().createConvertToVolumeOperator(t);
    dsource->addOperator(op);
    // dsource->setType(t);
    // }
  }
}

void SetDataTypeReaction::onTriggered()
{
  DataSource* dsource = ActiveObjects::instance().activeParentDataSource();
  setDataType(m_mainWindow, dsource, m_type);
  // setWidgetText(dsource);
}

void SetDataTypeReaction::updateEnableState()
{
  // auto dsource = ActiveObjects::instance().activeDataSource();
  auto pipeline = ActiveObjects::instance().activePipeline();
  bool enable = pipeline != nullptr;
  if (enable) {
    auto dsource = pipeline->transformedDataSource();
    enable = dsource != nullptr;
    if (enable) {
      enable = dsource->type() != m_type;
    }
  }
  parentAction()->setEnabled(enable);
}

void SetDataTypeReaction::setWidgetText(DataSource::DataSourceType t)
{
  if (t == DataSource::Volume) {
    parentAction()->setText("Mark Data As Volume");
  } else if (t == DataSource::TiltSeries) {
    parentAction()->setText("Mark Data As Tilt Series");
  } else if (t == DataSource::FIB) {
    parentAction()->setText("Mark Data As Focused Ion Beam (FIB)");
  } else {
    assert("Unknown data source type" && false);
  }
}
} // namespace tomviz
