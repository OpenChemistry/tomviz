/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizArrayWranglerReaction_h
#define tomvizArrayWranglerReaction_h

#include <pqReaction.h>

class QMainWindow;

namespace tomviz {
class DataSource;

class ArrayWranglerReaction : public pqReaction
{
  Q_OBJECT

public:
  ArrayWranglerReaction(QAction* parent, QMainWindow* mw);

  void wrangleArray(DataSource* source = nullptr);

protected:
  void updateEnableState() override;
  void onTriggered() override { wrangleArray(); }

private:
  Q_DISABLE_COPY(ArrayWranglerReaction)
  QMainWindow* m_mainWindow;
};
} // namespace tomviz

#endif
