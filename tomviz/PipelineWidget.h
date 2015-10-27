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
#ifndef tomvizPipelineWidget_h
#define tomvizPipelineWidget_h

#include <QTreeWidget>
#include <QScopedPointer>

class pqPipelineSource;
class vtkSMSourceProxy;
class vtkSMViewProxy;

namespace tomviz
{

class DataSource;
class Module;

/// PipelineWidget is a QTreeWidget to show the visualization "Pipeline" in
/// tomviz. This is not same as the underlying ParaView visualization pipeline.
/// We show higher level abstractions for tomviz users, than the raw VTK
/// pipelines.
class PipelineWidget : public QTreeWidget
{
  Q_OBJECT

  typedef QTreeWidget Superclass;

public:
  PipelineWidget(QWidget* parent=nullptr);
  virtual ~PipelineWidget();

  void keyPressEvent(QKeyEvent*) override;

private slots:
  /// Slots connected to pqServerManagerModel to monitor pipeline proxies
  /// being registered/unregistered.
  void dataSourceAdded(DataSource* producer);
  void dataSourceRemoved(DataSource* producer);

  /// Slots connected to ModuleManager to monitor modules.
  void moduleAdded(Module*);
  void moduleRemoved(Module*);

  /// Called when the ActiveObjects' active data source changes.
  void setCurrent(DataSource*);

  /// Called when the ActiveObjects's active module changes.
  void setCurrent(Module* module);

  /// Called when current item selected in the widget changes.
  void currentItemChanged(QTreeWidgetItem*);

  /// called when an item is clicked. We handle visibility toggling.
  void onItemClicked(QTreeWidgetItem*, int);

  /// called when the active view changes.
  void setActiveView(vtkSMViewProxy* view);

  /// called when a context menu is required
  void onCustomContextMenu(const QPoint &point);

private:
  Q_DISABLE_COPY(PipelineWidget)
  class PWInternals;
  QScopedPointer<PWInternals> Internals;
};

}

#endif
