/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "SetDataTypeReaction.h"

#include "ActiveObjects.h"
#include "OperatorFactory.h"
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
    Operator* op = OperatorFactory::createConvertToVolumeOperator(t);
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
  auto dsource = ActiveObjects::instance().activeDataSource();
  parentAction()->setEnabled(false);
  if (dsource != nullptr) {
    parentAction()->setEnabled(dsource->type() != m_type);
    // setWidgetText(dsource);
  }
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
