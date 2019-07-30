/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizReaction_h
#define tomvizReaction_h

#include <pqReaction.h>

namespace tomviz {

class Reaction : public pqReaction
{
  Q_OBJECT

public:
  Reaction(QAction* parent);

protected:
  void updateEnableState() override;
};
} // namespace tomviz
#endif
