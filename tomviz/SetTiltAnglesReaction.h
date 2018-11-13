/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSetTiltAnglesReaction_h
#define tomvizSetTiltAnglesReaction_h

#include <pqReaction.h>

class QMainWindow;

namespace tomviz {
class DataSource;

class SetTiltAnglesReaction : public pqReaction
{
  Q_OBJECT

public:
  SetTiltAnglesReaction(QAction* parent, QMainWindow* mw);

  static void showSetTiltAnglesUI(QMainWindow* window,
                                  DataSource* source = nullptr);

protected:
  void updateEnableState() override;
  void onTriggered() override { showSetTiltAnglesUI(m_mainWindow); }

private:
  Q_DISABLE_COPY(SetTiltAnglesReaction)
  QMainWindow* m_mainWindow;
};
} // namespace tomviz

#endif
