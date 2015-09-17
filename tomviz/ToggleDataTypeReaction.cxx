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

#include <cassert>

namespace tomviz
{

ToggleDataTypeReaction::ToggleDataTypeReaction(QAction* action)
  : pqReaction(action)
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateEnableState()));
  this->updateEnableState();
}

ToggleDataTypeReaction::~ToggleDataTypeReaction()
{
}

void ToggleDataTypeReaction::onTriggered()
{
  DataSource* dsource = ActiveObjects::instance().activeDataSource();
  if (dsource == NULL)
  {
    return;
  }
  if (dsource->type() == DataSource::Volume)
  {
    dsource->setType(DataSource::TiltSeries);
  }
  else if (dsource->type() == DataSource::TiltSeries)
  {
    dsource->setType(DataSource::Volume);
  }
  this->setWidgetText(dsource);
}

void ToggleDataTypeReaction::updateEnableState()
{
  DataSource* dsource = ActiveObjects::instance().activeDataSource();
  this->parentAction()->setEnabled(dsource != NULL);
  if (dsource != NULL)
  {
    this->setWidgetText(dsource);
  }
}

void ToggleDataTypeReaction::setWidgetText(DataSource* dsource)
{
  if (dsource->type() == DataSource::Volume)
  {
      this->parentAction()->setText("Mark As Tilt Series");
  }
  else if (dsource->type() == DataSource::TiltSeries)
  {
      this->parentAction()->setText("Mark As Volume");
  }
  else
  {
    assert("Unknown data source type" && false);
  }
}

}
