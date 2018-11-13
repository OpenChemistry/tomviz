/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
