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
#ifndef tomvizActiveObjects_h
#define tomvizActiveObjects_h

#include <QObject>
#include <QPointer>

#include "DataSource.h"
#include "Module.h"
#include "Operator.h"
#include "OperatorResult.h"

class pqView;
class vtkSMSessionProxyManager;
class vtkSMViewProxy;

namespace tomviz {

class Pipeline;

/// ActiveObjects keeps track of active objects in tomviz.
/// This is similar to pqActiveObjects in ParaView, however it tracks objects
/// relevant to tomviz.
class ActiveObjects : public QObject
{
  Q_OBJECT

public:
  /// Returns reference to the singleton instance.
  static ActiveObjects& instance();

  /// Returns the active view.
  vtkSMViewProxy* activeView() const;

  /// Returns the active data source.
  DataSource* activeDataSource() const { return m_activeDataSource; }

  /// Returns the selected data source, nullptr if no data source is selected.
  DataSource* selectedDataSource() const { return m_selectedDataSource; }

  /// Returns the active transformed data source.
  DataSource* activeTransformedDataSource() const
  {
    return m_activeTransformedDataSource;
  }

  /// Returns the active module.
  Module* activeModule() const { return m_activeModule; }

  /// Returns the active OperatorResult
  OperatorResult* activeOperatorResult() const
  {
    return m_activeOperatorResult;
  }

  /// Returns the vtkSMSessionProxyManager from the active server/session.
  /// Provided here for convenience, since we need to access the proxy manager
  /// often.
  vtkSMSessionProxyManager* proxyManager() const;

  bool moveObjectsEnabled() { return m_moveObjectsEnabled; }

  /// Returns the active pipelines.
  Pipeline* activePipeline() const;

  /// The "parent" data source is the data source that new operators will be
  /// appended to. i.e. The closes parent of the currently active data source
  /// that is not an "Output" data source.
  DataSource* activeParentDataSource();

public slots:
  /// Set the active view;
  void setActiveView(vtkSMViewProxy*);

  /// Set the active data source.
  void setActiveDataSource(DataSource* source);

  /// Set the selected data source.
  void setSelectedDataSource(DataSource* source);

  /// Set the active transformed data source.
  void setActiveTransformedDataSource(DataSource* source);

  /// Set the active module.
  void setActiveModule(Module* module);

  /// Set the active operator result.
  void setActiveOperatorResult(OperatorResult* result);

  /// Set the active operator.
  void setActiveOperator(Operator* op);

  /// Create a render view if needed.
  void createRenderViewIfNeeded();

  /// Set first existing render view to be active.
  void setActiveViewToFirstRenderView();

  /// Renders all views.
  void renderAllViews();

  /// Set the active mode (true for the mode where objects can
  /// be moved via MoveActiveObject)
  void setMoveObjectsMode(bool moveObjectsOn);

signals:
  /// Fired whenever the active view changes.
  void viewChanged(vtkSMViewProxy*);

  /// Fired whenever the active data source changes (or changes type).
  void dataSourceChanged(DataSource*);

  /// Fired whenever the data source is activated, i.e. selected in the
  /// pipeline.
  void dataSourceActivated(DataSource*);

  /// Fired whenever the data source is activated, i.e. selected in the
  /// pipeline. This signal emits the transformed data source.
  void transformedDataSourceActivated(DataSource*);

  /// Fired whenever the active module changes.
  void moduleChanged(Module*);

  /// Fired whenever a module is activated, i.e. selected in the pipeline.
  void moduleActivated(Module*);

  /// Fired whenever the active operator changes.
  void operatorChanged(Operator*);

  /// Fired whenever an operator is activated, i.e. selected in the pipeline.
  void operatorActivated(Operator*);

  /// Fired whenever the active operator changes.
  void resultChanged(OperatorResult*);

  /// Fired whenever an OperatorResult is activated.
  void resultActivated(OperatorResult*);

  /// Fired when the mode changes
  void moveObjectsModeChanged(bool moveObjectsOn);

  /// Fired whenever the color map has changed
  void colorMapChanged(DataSource*);

private slots:
  void viewChanged(pqView*);
  void dataSourceRemoved(DataSource*);
  void moduleRemoved(Module*);
  void dataSourceChanged();

protected:
  ActiveObjects();
  ~ActiveObjects() override;

  QPointer<DataSource> m_activeDataSource = nullptr;
  QPointer<DataSource> m_activeTransformedDataSource = nullptr;
  QPointer<DataSource> m_selectedDataSource = nullptr;
  DataSource::DataSourceType m_activeDataSourceType = DataSource::Volume;
  QPointer<DataSource> m_activeParentDataSource = nullptr;
  QPointer<Module> m_activeModule = nullptr;
  QPointer<Operator> m_activeOperator = nullptr;
  QPointer<OperatorResult> m_activeOperatorResult = nullptr;
  bool m_moveObjectsEnabled = false;

private:
  Q_DISABLE_COPY(ActiveObjects)
};
} // namespace tomviz

#endif
