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
#ifndef __TEM_CloneDataReaction_h
#define __TEM_CloneDataReaction_h

#include "pqReaction.h"

namespace TEM
{
class DataSource;

class CloneDataReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  CloneDataReaction(QAction* action);
  virtual ~CloneDataReaction();

  static DataSource* clone(DataSource* toClone = NULL);

protected:
  /// Called when the action is triggered.
  virtual void onTriggered() { this->clone(); }
  virtual void updateEnableState();

private:
  Q_DISABLE_COPY(CloneDataReaction)
};
}

#endif
