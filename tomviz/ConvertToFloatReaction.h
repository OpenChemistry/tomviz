/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConvertToFloatReaction_h
#define tomvizConvertToFloatReaction_h

#include <Reaction.h>

namespace tomviz {
class DataSource;

class ConvertToFloatReaction : public Reaction
{
  Q_OBJECT

public:
  ConvertToFloatReaction(QAction* parent);

  void convertToFloat();

protected:
  void onTriggered() override { convertToFloat(); }

private:
  Q_DISABLE_COPY(ConvertToFloatReaction)
};
} // namespace tomviz
#endif
