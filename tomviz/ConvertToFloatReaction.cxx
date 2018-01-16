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
#include "ConvertToFloatReaction.h"

#include <QDebug>

#include "ActiveObjects.h"
#include "ConvertToFloatOperator.h"
#include "DataSource.h"

namespace tomviz {

ConvertToFloatReaction::ConvertToFloatReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void ConvertToFloatReaction::updateEnableState()
{
  parentAction()->setEnabled(
    ActiveObjects::instance().activeParentDataSource() != nullptr);
}

void ConvertToFloatReaction::convertToFloat()
{
  DataSource* source = ActiveObjects::instance().activeParentDataSource();
  if (!source) {
    qDebug() << "Exiting early - no data found.";
    return;
  }
  Operator* Op = new ConvertToFloatOperator();

  source->addOperator(Op);
}
}
