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
#ifndef tomvizSaveScreenshotReaction_h
#define tomvizSaveScreenshotReaction_h

#include <pqReaction.h>

namespace tomviz {
class MainWindow;

class SaveScreenshotReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveScreenshotReaction(QAction* a, MainWindow* mw);

  static void saveScreenshot(MainWindow* mw);

protected:
  void onTriggered() override { saveScreenshot(m_mainWindow); }

private:
  Q_DISABLE_COPY(SaveScreenshotReaction)

  MainWindow* m_mainWindow;
};
} // namespace tomviz

#endif
