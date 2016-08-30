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
#ifndef tomvizDataTransformMenu_h
#define tomvizDataTransformMenu_h

#include <QObject>
#include <QPointer>
#include <QScopedPointer>

class QMainWindow;
class QMenu;

namespace tomviz {

// DataTransformMenu is the manager for the Data Transform menu.
// It is responsible for enabling and disabling Data Transforms based
// on properties of the DataSource.
class DataTransformMenu : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  DataTransformMenu(QMainWindow* mainWindow = nullptr, QMenu* menu = nullptr);
  virtual ~DataTransformMenu();

private slots:
  void updateActions();

protected slots:
  void buildMenu();

private:
  Q_DISABLE_COPY(DataTransformMenu)
  QPointer<QMenu> Menu;

  class DTInternals;
  QScopedPointer<DTInternals> Internals;
};
}

#endif
