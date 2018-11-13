/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAddResampleReaction_h
#define tomvizAddResampleReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;

class AddResampleReaction : public pqReaction
{
  Q_OBJECT

public:
  AddResampleReaction(QAction* parent);

  void resample(DataSource* source = nullptr);

protected:
  void updateEnableState() override;
  void onTriggered() override { resample(); }

private:
  Q_DISABLE_COPY(AddResampleReaction)
};
} // namespace tomviz

#endif
