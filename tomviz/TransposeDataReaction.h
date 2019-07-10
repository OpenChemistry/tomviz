/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTransposeDataReaction_h
#define tomvizTransposeDataReaction_h

#include <pqReaction.h>

class QMainWindow;

namespace tomviz {
class DataSource;

class TransposeDataReaction : public pqReaction
{
  Q_OBJECT

public:
  TransposeDataReaction(QAction* parent, QMainWindow* mw);

  void transposeData(DataSource* source = nullptr);

protected:
  void updateEnableState() override;
  void onTriggered() override { transposeData(); }

private:
  Q_DISABLE_COPY(TransposeDataReaction)
  QMainWindow* m_mainWindow;
};
} // namespace tomviz

#endif
