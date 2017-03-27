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
#ifndef tomvizSaveLoadStateReaction_h
#define tomvizSaveLoadStateReaction_h

#include <pqReaction.h>

namespace tomviz {

class SaveLoadStateReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveLoadStateReaction(QAction* action, bool load = false);

  static bool saveState();
  static bool saveState(const QString& filename, bool interactive = true);
  static bool loadState();
  static bool loadState(const QString& filename);

protected:
  void onTriggered() override;

private:
  Q_DISABLE_COPY(SaveLoadStateReaction)
  bool m_load;
};
}

#endif
