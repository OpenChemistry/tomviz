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

#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqRenderView.h"
#include "pqServerManagerModel.h"
#include "pqView.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMRenderViewProxy.h"

#include <QAction>
#include <QColor>
#include <QColorDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QWidget>

namespace tomviz
{
AddRenderViewContextMenuBehavior::AddRenderViewContextMenuBehavior(QObject* p)
  : Superclass(p)
{
  QObject::connect(
    pqApplicationCore::instance()->getServerManagerModel(),
    SIGNAL(viewAdded(pqView*)),
    this, SLOT(onViewAdded(pqView*)));
  this->menu = new QMenu();
  QAction* bgColorAction = this->menu->addAction("Set Background Color");
  QObject::connect(bgColorAction, SIGNAL(triggered()),
    this, SLOT(onSetBackgroundColor()));
}

AddRenderViewContextMenuBehavior::~AddRenderViewContextMenuBehavior()
{
  delete this->menu;
}

void AddRenderViewContextMenuBehavior::onViewAdded(pqView* view)
{
  if (view && view->getProxy()->IsA("vtkSMRenderViewProxy"))
  {
    // add a link view menu
    view->widget()->installEventFilter(this);
  }
}

void AddRenderViewContextMenuBehavior::onSetBackgroundColor()
{
  pqView* view = pqActiveObjects::instance().activeView();
  vtkSMRenderViewProxy* proxy = vtkSMRenderViewProxy::SafeDownCast(view->getProxy());
  vtkSMPropertyHelper helper(proxy, "Background");
  double colorComps[3];
  helper.Get(colorComps,3);

  QColor currentColor = QColor::fromRgbF(colorComps[0], colorComps[1], colorComps[2]);
  QColor c = QColorDialog::getColor(currentColor, view->widget(), "Select Color");

  colorComps[0] = c.redF();
  colorComps[1] = c.greenF();
  colorComps[2] = c.blueF();
  helper.Set(colorComps,3);

  proxy->UpdateVTKObjects();
  view->render();
}

// Copied straight from ParaView's pqPipelineContextMenuBehavior to show
// the right click menu only when the right click was not a click and drag
bool AddRenderViewContextMenuBehavior::eventFilter(QObject* caller, QEvent* e)
{
  if (e->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent* me = static_cast<QMouseEvent*>(e);
    if (me->button() & Qt::RightButton)
    {
      this->position = me->pos();
    }
  }
  else if (e->type() == QEvent::MouseButtonRelease)
  {
    QMouseEvent* me = static_cast<QMouseEvent*>(e);
    if (me->button() & Qt::RightButton && !this->position.isNull())
    {
      QPoint newPos = static_cast<QMouseEvent*>(e)->pos();
      QPoint delta = newPos - this->position;
      QWidget* senderWidget = qobject_cast<QWidget*>(caller);
      if (delta.manhattanLength() < 3 && senderWidget != NULL)
      {
        pqRenderView* view = qobject_cast<pqRenderView*>(
          pqActiveObjects::instance().activeView());
        if (view)
        {
          this->menu->popup(senderWidget->mapToGlobal(newPos));
        }
      }
      this->position = QPoint();
    }
  }

  return Superclass::eventFilter(caller, e);
}

}
