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
#ifndef tomvizSetTiltAnglesReaction_h
#define tomvizSetTiltAnglesReaction_h

#include "pqReaction.h"

class QMainWindow;

namespace tomviz
{
class DataSource;

class SetTiltAnglesReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  SetTiltAnglesReaction(QAction* parent, QMainWindow* mw);
  ~SetTiltAnglesReaction();

  static void showSetTiltAnglesUI(QMainWindow *window, DataSource *source = NULL);

protected:
  void updateEnableState();
  void onTriggered() { showSetTiltAnglesUI(this->mainWindow); }
private:
  Q_DISABLE_COPY(SetTiltAnglesReaction)
  QMainWindow *mainWindow;
};
}

#endif
