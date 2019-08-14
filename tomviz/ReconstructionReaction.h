/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizReconstructionReaction_h
#define tomvizReconstructionReaction_h

#include <Reaction.h>

namespace tomviz {
class DataSource;

class ReconstructionReaction : public Reaction
{
  Q_OBJECT

public:
  ReconstructionReaction(QAction* parent);

  void recon(DataSource* input = NULL);

protected:
  void onTriggered() { recon(); }

private:
  Q_DISABLE_COPY(ReconstructionReaction)
};
} // namespace tomviz

#endif
