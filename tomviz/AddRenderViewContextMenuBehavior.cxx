/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AddRenderViewContextMenuBehavior.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqCameraLinkReaction.h>
#include <pqLinkViewWidget.h>
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
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QWidget>

namespace tomviz {

namespace {

// ParaView's pqRenderView::linkToOtherView() connects QWidget::close, a
// slot, as if it were a signal, which warns "signal not found" on every
// link and never frees the widget. Show the widget here instead, and free
// it once it hides.
class LinkViewWidget : public pqLinkViewWidget
{
public:
  using pqLinkViewWidget::pqLinkViewWidget;

protected:
  bool event(QEvent* e) override
  {
    bool handled = pqLinkViewWidget::event(e);
    if (e->type() == QEvent::Hide) {
      deleteLater();
    }
    return handled;
  }
};

class CameraLinkReaction : public pqCameraLinkReaction
{
public:
  using pqCameraLinkReaction::pqCameraLinkReaction;

protected:
  void onTriggered() override
  {
    auto* view =
      qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());
    if (!view) {
      return;
    }
    auto* widget = new LinkViewWidget(view);
    widget->move(view->widget()->mapToGlobal(QPoint(2, 2)));
    widget->show();
  }
};

} // namespace

AddRenderViewContextMenuBehavior::AddRenderViewContextMenuBehavior(QObject* p)
  : QObject(p)
{
  connect(pqApplicationCore::instance()->getServerManagerModel(),
          &pqServerManagerModel::viewAdded, this,
          &AddRenderViewContextMenuBehavior::onViewAdded);
  m_menu = new QMenu();
  QAction* bgColorAction = m_menu->addAction("Set Background Color");
  connect(bgColorAction, &QAction::triggered, this,
          &AddRenderViewContextMenuBehavior::onSetBackgroundColor);

  // Add separator
  m_menu->addSeparator();

  // Support camera linking/unlinking
  new CameraLinkReaction(m_menu->addAction("Add Camera Link...")
                           << pqSetName("actionToolsAddCameraLink"));
  new pqManageLinksReaction(m_menu->addAction("Manage Camera Links...")
                            << pqSetName("actionToolsManageCameraLinks"));
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

  // Must set this to zero so that the render view will use its own
  // background color rather than the global palette.
  if (proxy->GetProperty("UseColorPaletteForBackground")) {
    vtkSMPropertyHelper(proxy, "UseColorPaletteForBackground").Set(0);
  }

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
} // namespace tomviz
