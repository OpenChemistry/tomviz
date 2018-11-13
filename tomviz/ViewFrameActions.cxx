/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ViewFrameActions.h"

namespace tomviz {

ViewFrameActions::ViewFrameActions(QObject* p)
  : pqStandardViewFrameActionsImplementation(p)
{}

QList<pqStandardViewFrameActionsImplementation::ViewType>
ViewFrameActions::availableViewTypes()
{
  QList<ViewType> views;
  QList<ViewType> availableViews =
    pqStandardViewFrameActionsImplementation::availableViewTypes();
  foreach (ViewType viewType, availableViews) {
    if (viewType.Name == "RenderView")
      views.push_back(viewType);
    else if (viewType.Name == "SpreadSheetView")
      views.push_back(viewType);
  }
  return views;
}

bool ViewFrameActions::isButtonVisible(const std::string& buttonName, pqView*)
{
  if (buttonName == "ForwardButton" || buttonName == "BackButton" ||
      buttonName == "ToggleInteractionMode" || buttonName == "AdjustCamera") {
    return true;
  }
  return false;
}
} // namespace tomviz
