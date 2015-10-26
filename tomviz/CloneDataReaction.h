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
#ifndef tomvizCloneDataReaction_h
#define tomvizCloneDataReaction_h

#include "pqReaction.h"

namespace tomviz
{
class DataSource;

class CloneDataReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  CloneDataReaction(QAction* action);
  virtual ~CloneDataReaction();

  static DataSource* clone(DataSource* toClone = nullptr);

protected:
  /// Called when the action is triggered.
  void onTriggered() override { this->clone(); }
  void updateEnableState() override;

private:
  Q_DISABLE_COPY(CloneDataReaction)
};
}

#endif
