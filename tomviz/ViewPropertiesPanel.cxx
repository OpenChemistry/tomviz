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
#include "ViewPropertiesPanel.h"
#include "ui_ViewPropertiesPanel.h"

#include "ActiveObjects.h"
#include "Utilities.h"
#include "pqProxiesWidget.h"
#include "vtkSMViewProxy.h"
#include "pqView.h"

namespace tomviz
{

class ViewPropertiesPanel::VPPInternals
{
public:
  Ui::ViewPropertiesPanel Ui;

  VPPInternals()
    {
    }
};

//-----------------------------------------------------------------------------
ViewPropertiesPanel::ViewPropertiesPanel(QWidget* parentObject)
  : Superclass(parentObject),
    Internals(new ViewPropertiesPanel::VPPInternals())
{
  Ui::ViewPropertiesPanel &ui = this->Internals->Ui;
  ui.setupUi(this);

  this->connect(ui.SearchBox, SIGNAL(advancedSearchActivated(bool)),
                SLOT(updatePanel()));
  this->connect(ui.SearchBox, SIGNAL(textChanged(const QString&)),
                SLOT(updatePanel()));
  this->connect(&ActiveObjects::instance(),
                SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(setView(vtkSMViewProxy*)));
  this->connect(ui.ProxiesWidget, SIGNAL(changeFinished(vtkSMProxy*)),
                SLOT(render()));
}

//-----------------------------------------------------------------------------
ViewPropertiesPanel::~ViewPropertiesPanel()
{
}

//-----------------------------------------------------------------------------
void ViewPropertiesPanel::setView(vtkSMViewProxy* view)
{
  Ui::ViewPropertiesPanel &ui = this->Internals->Ui;
  ui.ProxiesWidget->clear();
  if (view)
    {
    ui.ProxiesWidget->addProxy(view, view->GetXMLLabel(), QStringList(), true);
    }
  ui.ProxiesWidget->updateLayout();
  this->updatePanel();
}

//-----------------------------------------------------------------------------
void ViewPropertiesPanel::render()
{
  pqView* view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view)
    {
    view->render();
    }
}

//-----------------------------------------------------------------------------
void ViewPropertiesPanel::updatePanel()
{
  Ui::ViewPropertiesPanel &ui = this->Internals->Ui;
  ui.ProxiesWidget->filterWidgets(
    ui.SearchBox->isAdvancedSearchActive(),
    ui.SearchBox->text());
}

}
