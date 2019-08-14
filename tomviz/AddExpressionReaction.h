/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAddExpressionReaction_h
#define tomvizAddExpressionReaction_h

#include <Reaction.h>

namespace tomviz {
class DataSource;
class OperatorPython;

class AddExpressionReaction : public Reaction
{
  Q_OBJECT

public:
  AddExpressionReaction(QAction* parent);

  OperatorPython* addExpression(DataSource* source = nullptr);

protected:
  void onTriggered() override { addExpression(); }

private:
  Q_DISABLE_COPY(AddExpressionReaction)

  QString getDefaultExpression(DataSource*);
};
} // namespace tomviz

#endif
