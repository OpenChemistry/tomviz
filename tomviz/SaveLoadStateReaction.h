/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __TEM_SaveLoadStateReaction_h
#define __TEM_SaveLoadStateReaction_h

#include "pqReaction.h"

namespace TEM
{

class SaveLoadStateReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  SaveLoadStateReaction(QAction* action, bool load=false);
  virtual ~SaveLoadStateReaction();

  static bool saveState();
  static bool saveState(const QString& filename);
  static bool loadState();
  static bool loadState(const QString& filename);

protected:
  virtual void onTriggered();

private:
  Q_DISABLE_COPY(SaveLoadStateReaction)
  bool Load;
};

}

#endif
