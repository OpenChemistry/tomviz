/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizViewFrameActions_h
#define tomvizViewFrameActions_h

#include <pqStandardViewFrameActionsImplementation.h>

namespace tomviz {

class ViewFrameActions : public pqStandardViewFrameActionsImplementation
{
  Q_OBJECT
  Q_INTERFACES(pqViewFrameActionsInterface)

public:
  explicit ViewFrameActions(QObject* parent = nullptr);

protected:
  QList<ViewType> availableViewTypes() override;
  bool isButtonVisible(const std::string& buttonName, pqView* view) override;

private:
  Q_DISABLE_COPY(ViewFrameActions)
};
} // namespace tomviz

#endif // tomvizViewFrameActions_h
