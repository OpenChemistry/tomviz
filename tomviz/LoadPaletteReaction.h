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
#ifndef tomvizLoadPaletteReaction_h
#define tomvizLoadPaletteReaction_h

#include <pqReaction.h>

#include <QPointer>
#include <QStringList>

class QAction;

namespace tomviz {

class LoadPaletteReaction : public pqReaction
{
  Q_OBJECT

public:
  LoadPaletteReaction(QAction* parentObject = nullptr);
  ~LoadPaletteReaction() override;

private slots:
  void populateMenu();
  void actionTriggered(QAction* actn);

private:
  Q_DISABLE_COPY(LoadPaletteReaction)
  QPointer<QMenu> m_menu;
  QStringList m_paletteWhiteList;
};
}
#endif
