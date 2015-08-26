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
#ifndef tomvizMainWindow_h
#define tomvizMainWindow_h

#include <QMainWindow>

namespace tomviz
{

class DataPropertiesPanel;
class DataSource;
class Module;
class ModulePropertiesPanel;

/// The main window for the tomviz application.
class MainWindow : public QMainWindow
{
  Q_OBJECT

  typedef QMainWindow Superclass;

public:
  MainWindow(QWidget* parent=0, Qt::WindowFlags flags=0);
  virtual ~MainWindow();

private slots:
  void showAbout();
  void openTilt();
  void openRecon();

  /// Change the active data source in the UI.
  void dataSourceChanged(DataSource *source);
  /// Change the active module displayed in the UI.
  void moduleChanged(Module *module);

private:
  Q_DISABLE_COPY(MainWindow)
  class MWInternals;
  MWInternals* Internals;

  DataPropertiesPanel *DataPropertiesWidget;
  ModulePropertiesPanel *ModulePropertiesWidget;
};

}
#endif
