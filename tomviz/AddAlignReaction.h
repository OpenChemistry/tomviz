/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAddAlignReaction_h
#define tomvizAddAlignReaction_h

#include <Reaction.h>

namespace tomviz {
class DataSource;

class AddAlignReaction : public Reaction
{
  Q_OBJECT

public:
  AddAlignReaction(QAction* parent);

  void align(DataSource* source = nullptr);

protected:
  void onTriggered() override { align(); }

private:
  Q_DISABLE_COPY(AddAlignReaction)
};
} // namespace tomviz

#endif
