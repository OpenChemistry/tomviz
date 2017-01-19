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

#include <pqLoadPaletteReaction.h>

#include <QStringList>

class QAction;

namespace tomviz {

class LoadPaletteReaction : public pqLoadPaletteReaction
{
  Q_OBJECT

public:
  LoadPaletteReaction(QAction* parentObject = nullptr);
  ~LoadPaletteReaction() override;

private slots:
  void populateMenu();

private:
  Q_DISABLE_COPY(LoadPaletteReaction)

  QStringList m_paletteWhiteList;
};
}
#endif
