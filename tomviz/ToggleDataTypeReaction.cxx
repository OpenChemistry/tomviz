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

#include "ToggleDataTypeReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "OperatorFactory.h"
#include "SetTiltAnglesReaction.h"

#include <cassert>

namespace tomviz {

ToggleDataTypeReaction::ToggleDataTypeReaction(QAction* action, QMainWindow* mw)
  : pqReaction(action), mainWindow(mw)
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateEnableState()));
  this->updateEnableState();
}

ToggleDataTypeReaction::~ToggleDataTypeReaction()
{
}

void ToggleDataTypeReaction::toggleDataType(QMainWindow* mw,
                                            DataSource* dsource)
{
  if (dsource == nullptr) {
    dsource = ActiveObjects::instance().activeDataSource();
  }
  // if it is still null, give up
  if (dsource == nullptr) {
    return;
  }
  if (dsource->type() == DataSource::Volume) {
    SetTiltAnglesReaction::showSetTiltAnglesUI(mw, dsource);
  } else if (dsource->type() == DataSource::TiltSeries) {
    Operator* op = OperatorFactory::createConvertToVolumeOperator();
    dsource->addOperator(op);
  }
}

void ToggleDataTypeReaction::onTriggered()
{
  DataSource* dsource = ActiveObjects::instance().activeDataSource();
  toggleDataType(this->mainWindow, dsource);
  this->setWidgetText(dsource);
}

void ToggleDataTypeReaction::updateEnableState()
{
  DataSource* dsource = ActiveObjects::instance().activeDataSource();
  this->parentAction()->setEnabled(dsource != nullptr);
  if (dsource != nullptr) {
    this->setWidgetText(dsource);
  }
}

void ToggleDataTypeReaction::setWidgetText(DataSource* dsource)
{
  if (dsource->type() == DataSource::Volume) {
    this->parentAction()->setText("Mark Data As Tilt Series");
  } else if (dsource->type() == DataSource::TiltSeries) {
    this->parentAction()->setText("Mark Data As Volume");
  } else {
    assert("Unknown data source type" && false);
  }
}
}
