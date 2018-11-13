/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCropReaction_h
#define tomvizCropReaction_h

#include <pqReaction.h>

class QMainWindow;

namespace tomviz {
class DataSource;

class CropReaction : public pqReaction
{
  Q_OBJECT

public:
  CropReaction(QAction* parent, QMainWindow* mw);

  void crop(DataSource* source = nullptr);

protected:
  void updateEnableState() override;
  void onTriggered() override { crop(); }

private:
  Q_DISABLE_COPY(CropReaction)
  QMainWindow* m_mainWindow;
};
} // namespace tomviz

#endif
