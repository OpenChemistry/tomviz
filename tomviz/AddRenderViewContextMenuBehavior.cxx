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

#include "AddRenderViewContextMenuBehavior.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqCameraLinkReaction.h>
#include <pqManageLinksReaction.h>
#include <pqRenderView.h>
#include <pqServerManagerModel.h>
#include <pqSetName.h>
#include <pqView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMRenderViewProxy.h>

#include <QAction>
#include <QColor>
#include <QColorDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QWidget>

namespace tomviz {
AddRenderViewContextMenuBehavior::AddRenderViewContextMenuBehavior(QObject* p)
  : QObject(p)
{
  connect(pqApplicationCore::instance()->getServerManagerModel(),
          SIGNAL(viewAdded(pqView*)), SLOT(onViewAdded(pqView*)));
  m_menu = new QMenu();
  QAction* bgColorAction = m_menu->addAction("Set Background Color");
  connect(bgColorAction, SIGNAL(triggered()), SLOT(onSetBackgroundColor()));

  // Add separator
  m_menu->addSeparator();

  // Support camera linking/unlinking
  new pqCameraLinkReaction(
    m_menu->addAction("Add Camera Link...") << pqSetName("actionToolsAddCameraLink"));
  new pqManageLinksReaction(
    m_menu->addAction("Manage Camera Links...") << pqSetName("actionToolsManageCameraLinks"));
}

AddRenderViewContextMenuBehavior::~AddRenderViewContextMenuBehavior()
{
  delete m_menu;
}

void AddRenderViewContextMenuBehavior::onViewAdded(pqView* view)
{
  if (view && view->getProxy()->IsA("vtkSMRenderViewProxy")) {
    // add a link view menu
    view->widget()->installEventFilter(this);
  }
}

void AddRenderViewContextMenuBehavior::onSetBackgroundColor()
{
  pqView* view = pqActiveObjects::instance().activeView();
  auto proxy = vtkSMRenderViewProxy::SafeDownCast(view->getProxy());
  vtkSMPropertyHelper helper(proxy, "Background");
  double colorComps[3];
  helper.Get(colorComps, 3);

  QColor currentColor =
    QColor::fromRgbF(colorComps[0], colorComps[1], colorComps[2]);
  QColor c =
    QColorDialog::getColor(currentColor, view->widget(), "Select Color");

  if (!c.isValid()) {
    return;
  }
  colorComps[0] = c.redF();
  colorComps[1] = c.greenF();
  colorComps[2] = c.blueF();
  helper.Set(colorComps, 3);

  proxy->UpdateVTKObjects();
  view->render();
}

// Copied straight from ParaView's pqPipelineContextMenuBehavior to show
// the right click menu only when the right click was not a click and drag
bool AddRenderViewContextMenuBehavior::eventFilter(QObject* caller, QEvent* e)
{
  if (e->type() == QEvent::MouseButtonPress) {
    auto me = static_cast<QMouseEvent*>(e);
    if (me->button() & Qt::RightButton) {
      m_position = me->pos();
    }
  } else if (e->type() == QEvent::MouseButtonRelease) {
    QMouseEvent* me = static_cast<QMouseEvent*>(e);
    if (me->button() & Qt::RightButton && !m_position.isNull()) {
      QPoint newPos = static_cast<QMouseEvent*>(e)->pos();
      QPoint delta = newPos - m_position;
      QWidget* senderWidget = qobject_cast<QWidget*>(caller);
      if (delta.manhattanLength() < 3 && senderWidget != nullptr) {
        pqRenderView* view =
          qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());
        if (view) {
          m_menu->popup(senderWidget->mapToGlobal(newPos));
        }
      }
      m_position = QPoint();
    }
  }

  return QObject::eventFilter(caller, e);
}
}
