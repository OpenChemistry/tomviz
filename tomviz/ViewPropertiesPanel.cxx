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
#include "pqView.h"
#include "vtkSMViewProxy.h"

namespace tomviz {

ViewPropertiesPanel::ViewPropertiesPanel(QWidget* parentObject)
  : QWidget(parentObject), m_ui(new Ui::ViewPropertiesPanel)
{
  m_ui->setupUi(this);

  this->connect(m_ui->SearchBox, SIGNAL(advancedSearchActivated(bool)),
                SLOT(updatePanel()));
  this->connect(m_ui->SearchBox, SIGNAL(textChanged(const QString&)),
                SLOT(updatePanel()));
  this->connect(&ActiveObjects::instance(),
                SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(setView(vtkSMViewProxy*)));
  this->connect(m_ui->ProxiesWidget, SIGNAL(changeFinished(vtkSMProxy*)),
                SLOT(render()));
}

ViewPropertiesPanel::~ViewPropertiesPanel() = default;

void ViewPropertiesPanel::setView(vtkSMViewProxy* view)
{
  m_ui->ProxiesWidget->clear();
  if (view) {
    m_ui->ProxiesWidget->addProxy(view, view->GetXMLLabel(), QStringList(), true);
  }
  m_ui->ProxiesWidget->updateLayout();
  this->updatePanel();
}

void ViewPropertiesPanel::render()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}

void ViewPropertiesPanel::updatePanel()
{
  m_ui->ProxiesWidget->filterWidgets(m_ui->SearchBox->isAdvancedSearchActive(),
                                     m_ui->SearchBox->text());
}
}
