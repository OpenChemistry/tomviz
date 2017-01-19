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
#include "LoadPaletteReaction.h"

#include <QAction>
#include <QMenu>

#include <pqActiveObjects.h>

namespace tomviz {

LoadPaletteReaction::LoadPaletteReaction(QAction* parentObject)
  : pqLoadPaletteReaction(parentObject)
{
  m_paletteWhiteList << "Default Background"
                     << "Black Background"
                     << "White Background";
  QMenu* menu = parentObject->menu();
  connect(menu, SIGNAL(aboutToShow()), SLOT(populateMenu()));
}

LoadPaletteReaction::~LoadPaletteReaction()
{
}

void LoadPaletteReaction::populateMenu()
{
  QMenu* menu = qobject_cast<QMenu*>(this->sender());
  QList<QAction*> actions = menu->actions();
  for (int i = 0; i < actions.size(); ++i) {
    QAction* action = actions[i];
    if (m_paletteWhiteList.indexOf(action->text()) < 0) {
      menu->removeAction(action);
    }
  }
}
}
