/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCropReaction_h
#define tomvizCropReaction_h

#include <Reaction.h>

class QMainWindow;

namespace tomviz {
class DataSource;

class CropReaction : public Reaction
{
  Q_OBJECT

public:
  CropReaction(QAction* parent, QMainWindow* mw);

  void crop(DataSource* source = nullptr);

protected:
  void onTriggered() override { crop(); }

private:
  Q_DISABLE_COPY(CropReaction)
  QMainWindow* m_mainWindow;
};
} // namespace tomviz

#endif
