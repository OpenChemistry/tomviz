/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizActiveObjects_h
#define tomvizActiveObjects_h

#include <QObject>
#include <QPointer>

#include "DataSource.h"
#include "Module.h"
#include "MoleculeSource.h"
#include "Operator.h"
#include "OperatorResult.h"

class pqRenderView;
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

  /// Returns the active view as a pqView object.
  pqView* activePqView() const;

  /// Returns the active pqRenderView object.
  pqRenderView* activePqRenderView() const;

  /// Returns the active data source.
  DataSource* activeDataSource() const { return m_activeDataSource; }

  /// Returns the selected data source, nullptr if no data source is selected.
  DataSource* selectedDataSource() const { return m_selectedDataSource; }

  /// Returns the active data source.
  MoleculeSource* activeMoleculeSource() const
  {
    return m_activeMoleculeSource;
  }

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

  /// Set the active molecule source
  void setActiveMoleculeSource(MoleculeSource* source);

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

  /// Edit interaction modes for all data sources
  void enableTranslation(bool b);
  void enableRotation(bool b);
  void enableScaling(bool b);

  bool translationEnabled() const { return m_translationEnabled; }
  bool rotationEnabled() const { return m_rotationEnabled; }
  bool scalingEnabled() const { return m_scalingEnabled; }

  void setFixedInteractionDataSource(DataSource* ds)
  {
    m_fixedInteractionDataSource = ds;
    emit interactionDataSourceFixed(ds);
  }

  DataSource* fixedInteractionDataSource() const
  {
    return m_fixedInteractionDataSource;
  };

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
  void moleculeSourceChanged(MoleculeSource*);

  /// Fired whenever a module is activated, i.e. selected in the pipeline.
  void moleculeSourceActivated(MoleculeSource*);

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

  /// Fired when interaction modes change
  void translationStateChanged(bool b);
  void rotationStateChanged(bool b);
  void scalingStateChanged(bool b);

  /// Fired whenever the color map has changed
  void colorMapChanged(DataSource*);

  /// Fired to set image viewer mode
  void setImageViewerMode(bool b);

  /// Fired when the interaction data source was fixed.
  void interactionDataSourceFixed(DataSource*);

private slots:
  void viewChanged(pqView*);
  void dataSourceRemoved(DataSource*);
  void moleculeSourceRemoved(MoleculeSource*);
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
  QPointer<MoleculeSource> m_activeMoleculeSource = nullptr;
  QPointer<Module> m_activeModule = nullptr;
  QPointer<Operator> m_activeOperator = nullptr;
  QPointer<OperatorResult> m_activeOperatorResult = nullptr;
  QPointer<DataSource> m_fixedInteractionDataSource = nullptr;

  /// interaction states
  bool m_translationEnabled = false;
  bool m_rotationEnabled = false;
  bool m_scalingEnabled = false;

private:
  Q_DISABLE_COPY(ActiveObjects)
};

inline void ActiveObjects::enableTranslation(bool b)
{
  if (m_translationEnabled == b) {
    return;
  }

  m_translationEnabled = b;
  emit translationStateChanged(b);
}

inline void ActiveObjects::enableRotation(bool b)
{
  if (m_rotationEnabled == b) {
    return;
  }

  m_rotationEnabled = b;
  emit rotationStateChanged(b);
}

inline void ActiveObjects::enableScaling(bool b)
{
  if (m_scalingEnabled == b) {
    return;
  }

  m_scalingEnabled = b;
  emit scalingStateChanged(b);
}

} // namespace tomviz

#endif
