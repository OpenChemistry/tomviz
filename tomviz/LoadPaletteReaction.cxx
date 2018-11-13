/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LoadPaletteReaction.h"

#include <QAction>
#include <QDebug>
#include <QMenu>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqApplicationSettingsReaction.h>
#include <pqUndoStack.h>

#include <vtkPVProxyDefinitionIterator.h>
#include <vtkSMGlobalPropertiesProxy.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyDefinitionManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSettings.h>
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
  connect(m_menu, SIGNAL(aboutToShow()), SLOT(populateMenu()));
  connect(&pqActiveObjects::instance(), SIGNAL(serverChanged(pqServer*)),
          SLOT(updateEnableState()));
  connect(m_menu, SIGNAL(triggered(QAction*)), SLOT(actionTriggered(QAction*)));
}

LoadPaletteReaction::~LoadPaletteReaction()
{
  if (QAction* pa = parentAction()) {
    pa->setMenu(nullptr);
  }
  delete m_menu;
}

void LoadPaletteReaction::populateMenu()
{
  QMenu* menu = qobject_cast<QMenu*>(sender());
  Q_ASSERT(menu);
  menu->clear();

  auto pxm = pqActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  auto pdmgr = pxm->GetProxyDefinitionManager();
  Q_ASSERT(pdmgr);

  // Add "DefaultBackground" as the first entry.
  if (pxm->GetPrototypeProxy("palettes", "DefaultBackground")) {
    auto actn = menu->addAction("Gray Background");
    actn->setProperty("PV_XML_GROUP", "palettes");
    actn->setProperty("PV_XML_NAME", "DefaultBackground");
  }

  vtkSmartPointer<vtkPVProxyDefinitionIterator> iter;
  iter.TakeReference(pdmgr->NewSingleGroupIterator("palettes"));
  for (iter->InitTraversal(); !iter->IsDoneWithTraversal();
       iter->GoToNextItem()) {
    if (auto prototype =
          pxm->GetPrototypeProxy("palettes", iter->GetProxyName())) {
      if (strcmp(prototype->GetXMLName(), "DefaultBackground") == 0 ||
          m_paletteWhiteList.indexOf(prototype->GetXMLLabel()) < 0) {
        // skip DefaultBackground since already added.
        continue;
      }
      auto actn = menu->addAction(prototype->GetXMLLabel());
      actn->setProperty("PV_XML_GROUP", "palettes");
      actn->setProperty("PV_XML_NAME", iter->GetProxyName());
    }
  }
  m_menu->addSeparator();
  m_menu->addAction("Make Current Palette Default");
}

void LoadPaletteReaction::actionTriggered(QAction* action)
{
  auto pxm = pqActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  auto paletteProxy = pxm->GetProxy("global_properties", "ColorPalette");

  if (action->property("PV_XML_NAME").isValid()) {
    // Setting the color property unlinks the global palette background property
    // from the view background property. As a result, changes to the palette
    // do not update the view background. To solve this, we re-link the global
    // palette background color property to the background.
    auto gbPaletteProxy =
      vtkSMGlobalPropertiesProxy::SafeDownCast(paletteProxy);
    Q_ASSERT(gbPaletteProxy);

    auto view = pqActiveObjects::instance().activeView();
    auto viewProxy = view->getProxy();

    auto linkedPropertyName =
      gbPaletteProxy->GetLinkedPropertyName(viewProxy, "Background");
    if (!linkedPropertyName) {
      if (!gbPaletteProxy->Link("BackgroundColor", viewProxy, "Background")) {
        qWarning() << "Failed to setup Background property link.";
      }
    }

    auto palettePrototype = pxm->GetPrototypeProxy(
      "palettes", action->property("PV_XML_NAME").toString().toLatin1().data());
    Q_ASSERT(palettePrototype);

    BEGIN_UNDO_SET("Load color palette");
    paletteProxy->Copy(palettePrototype);
    paletteProxy->UpdateVTKObjects();
    END_UNDO_SET();

    pqApplicationCore::instance()->render();
  } else if (action->text() == tr("Make Current Palette Default")) {
    auto settings = vtkSMSettings::GetInstance();
    settings->SetProxySettings(paletteProxy);
  }
}
} // namespace tomviz
