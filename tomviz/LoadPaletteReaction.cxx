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
#include "LoadPaletteReaction.h"

#include <QAction>
#include <QMenu>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqApplicationSettingsReaction.h>
#include <pqUndoStack.h>
#include <vtkPVProxyDefinitionIterator.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyDefinitionManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSmartPointer.h>

namespace tomviz {

LoadPaletteReaction::LoadPaletteReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  m_paletteWhiteList << "Default Background"
                     << "Black Background"
                     << "White Background";

  m_menu = new QMenu();
  m_menu->setObjectName("LoadPaletteMenu");
  parentObject->setMenu(m_menu);
  this->connect(m_menu, SIGNAL(aboutToShow()), SLOT(populateMenu()));
  this->connect(&pqActiveObjects::instance(), SIGNAL(serverChanged(pqServer*)),
                SLOT(updateEnableState()));
  this->connect(m_menu, SIGNAL(triggered(QAction*)),
                SLOT(actionTriggered(QAction*)));
}

LoadPaletteReaction::~LoadPaletteReaction()
{
  if (QAction* pa = this->parentAction()) {
    pa->setMenu(nullptr);
  }
  delete m_menu;
}

void LoadPaletteReaction::populateMenu()
{
  QMenu* menu = qobject_cast<QMenu*>(this->sender());
  menu->clear();

  vtkSMSessionProxyManager* pxm = pqActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  vtkSMProxyDefinitionManager* pdmgr = pxm->GetProxyDefinitionManager();
  Q_ASSERT(pdmgr);

  // Add "DefaultBackground" as the first entry.
  if (vtkSMProxy* prototype =
        pxm->GetPrototypeProxy("palettes", "DefaultBackground")) {
    QAction* actn = menu->addAction(prototype->GetXMLLabel());
    actn->setProperty("PV_XML_GROUP", "palettes");
    actn->setProperty("PV_XML_NAME", "DefaultBackground");
  }

  vtkSmartPointer<vtkPVProxyDefinitionIterator> iter;
  iter.TakeReference(pdmgr->NewSingleGroupIterator("palettes"));
  for (iter->InitTraversal(); !iter->IsDoneWithTraversal();
       iter->GoToNextItem()) {
    if (vtkSMProxy* prototype =
          pxm->GetPrototypeProxy("palettes", iter->GetProxyName())) {
      if (strcmp(prototype->GetXMLName(), "DefaultBackground") == 0 ||
          m_paletteWhiteList.indexOf(prototype->GetXMLLabel()) < 0) {
        // skip DefaultBackground since already added.
        continue;
      }
      QAction* actn = menu->addAction(prototype->GetXMLLabel());
      actn->setProperty("PV_XML_GROUP", "palettes");
      actn->setProperty("PV_XML_NAME", iter->GetProxyName());
    }
  }
}

void LoadPaletteReaction::actionTriggered(QAction* actn)
{
  if (actn->property("PV_XML_NAME").isValid()) {
    vtkSMSessionProxyManager* pxm = pqActiveObjects::instance().proxyManager();
    Q_ASSERT(pxm);

    vtkSMProxy* paletteProxy =
      pxm->GetProxy("global_properties", "ColorPalette");

    vtkSMProxy* palettePrototype = pxm->GetPrototypeProxy(
      "palettes", actn->property("PV_XML_NAME").toString().toLatin1().data());
    Q_ASSERT(palettePrototype);

    BEGIN_UNDO_SET("Load color palette");
    paletteProxy->Copy(palettePrototype);
    paletteProxy->UpdateVTKObjects();
    END_UNDO_SET();

    pqApplicationCore::instance()->render();
  }
}
}
