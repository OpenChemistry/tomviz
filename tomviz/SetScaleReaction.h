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
#ifndef tomvizSetScaleReaction_h
#define tomvizSetScaleReaction_h

#include "pqReaction.h"

namespace TEM
{
class SetScaleReaction : public pqReaction
{
  Q_OBJECT

public:
  SetScaleReaction(QAction* action);
  virtual ~SetScaleReaction();

  static void setScale();

protected:
  virtual void onTriggered() { setScale(); }

private:
  Q_DISABLE_COPY(SetScaleReaction)
};

}
#endif
