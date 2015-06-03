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
#ifndef tomvizSaveDataReaction_h
#define tomvizSaveDataReaction_h

#include "pqReaction.h"

namespace TEM
{
class DataSource;

/// SaveDataReaction handles the "Save Data" action in tomviz. On trigger,
/// this will save the data file.
///
class SaveDataReaction : public pqReaction
{
  Q_OBJECT

  typedef pqReaction Superclass;

public:
  SaveDataReaction(QAction* parentAction);
  virtual ~SaveDataReaction();

  /// Save the file
  bool saveData(const QString &filename);

protected:
  /// Called when the action is triggered.
  virtual void onTriggered();

private:
  Q_DISABLE_COPY(SaveDataReaction)
};

}
#endif
