/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConvertToFloatReaction_h
#define tomvizConvertToFloatReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;

class ConvertToFloatReaction : public pqReaction
{
  Q_OBJECT

public:
  ConvertToFloatReaction(QAction* parent);

  void convertToFloat();

protected:
  void updateEnableState() override;
  void onTriggered() override { convertToFloat(); }

private:
  Q_DISABLE_COPY(ConvertToFloatReaction)
};
} // namespace tomviz
#endif
