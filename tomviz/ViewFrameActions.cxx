/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "ViewFrameActions.h"

namespace tomviz {

ViewFrameActions::ViewFrameActions(QObject* p)
  : pqStandardViewFrameActionsImplementation(p)
{
}

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
}
