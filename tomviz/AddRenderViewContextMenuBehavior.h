/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAddRenderViewContextMenuBehavior_h
#define tomvizAddRenderViewContextMenuBehavior_h

#include <QObject>

#include <QPoint>

class QMenu;
class pqView;

namespace tomviz {
class AddRenderViewContextMenuBehavior : public QObject
{
  Q_OBJECT

public:
  AddRenderViewContextMenuBehavior(QObject* p);
  ~AddRenderViewContextMenuBehavior() override;

protected slots:
  void onViewAdded(pqView* view);

  void onSetBackgroundColor();

protected:
  bool eventFilter(QObject* caller, QEvent* e) override;

  QPoint m_position;
  QMenu* m_menu;
};
} // namespace tomviz

#endif
