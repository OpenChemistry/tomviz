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
#ifndef tomvizAddRenderViewContextMenuBehavior_h
#define tomvizAddRenderViewContextMenuBehavior_h

#include <QObject>
#include <QPoint>

class QMenu;
class pqView;

namespace tomviz
{
class AddRenderViewContextMenuBehavior : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;
public:
  AddRenderViewContextMenuBehavior(QObject* p);
  ~AddRenderViewContextMenuBehavior();

protected slots:
  void onViewAdded(pqView* view);

  void onSetBackgroundColor();

protected:
  virtual bool eventFilter(QObject* caller, QEvent* e);

  QPoint position;
  QMenu* menu;
};

}

#endif
