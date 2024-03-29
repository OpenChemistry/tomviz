/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleManager_h
#define tomvizModuleManager_h

#include <QObject>

#include "Module.h"

#include <QJsonObject>
#include <QMap>
#include <QScopedPointer>

class pqView;
class vtkSMSourceProxy;
class vtkSMViewProxy;
class vtkPVXMLElement;
class vtkSMProxyLocator;
class vtkSMRepresentationProxy;
class QDir;

namespace tomviz {
class DataSource;
class MoleculeSource;
class Module;
class Operator;
class Pipeline;

/// Singleton akin to ProxyManager, but to keep track (and
/// serialize/deserialze) modules.
class ModuleManager : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  static ModuleManager& instance();

  /// Returns a list of modules of the specified type showing the dataSource in
  /// the give view. If view is NULL, all modules for the dataSource will be
  /// returned.
  template <class T>
  QList<T> findModules(DataSource* dataSource, vtkSMViewProxy* view)
  {
    QList<T> modulesT;
    QList<Module*> modules = this->findModulesGeneric(dataSource, view);
    foreach (Module* module, modules) {
      // Don't include modules that are actually children of an OperatorResult
      if (module->operatorResult() == nullptr) {
        if (T moduleT = qobject_cast<T>(module)) {
          modulesT.push_back(moduleT);
        }
      }
    }
    return modulesT;
  }

  QList<DataSource*> dataSources();
  QList<DataSource*> childDataSources();
  QList<DataSource*> allDataSources();

  // Return the data sources in a Depth First Search (DFS) order,
  // so that child data sources will come immediately after their
  // parent data source in the list.
  // This *should* match the order of data sources in the pipeline view.
  QList<DataSource*> allDataSourcesDepthFirst();

  // Generate a list of unique labels from the list of data sources.
  // Duplicate labels will have a space and number appended to them,
  // such as "Label", "Label (2)", "Label (3)", etc.
  // The labels will be in the same order as the input DataSource list.
  static QStringList createUniqueLabels(const QList<DataSource*>& sources);

  QList<Module*> findModulesGeneric(const DataSource* dataSource,
                                    const vtkSMViewProxy* view);

  QList<Module*> findModulesGeneric(const MoleculeSource* dataSource,
                                    const vtkSMViewProxy* view);

  /// Save the application state as JSON, use stateDir as the base for relative
  /// paths.
  bool serialize(QJsonObject& doc, const QDir& stateDir,
                 bool interactive = true) const;
  bool deserialize(const QJsonObject& doc, const QDir& stateDir,
                   bool loadDataSources = true);

  /// Set the views from the provided state.
  /// Usually, this should be done after ModuleManager::deserialize().
  void setViews(const QJsonArray& views);

  /// Test if any data source has running operators
  bool hasRunningOperators();

  /// Return whether a DataSource is a child DataSource
  bool isChild(DataSource*) const;

  /// Used to lookup a view by id, only intended for use during deserialization.
  vtkSMViewProxy* lookupView(int id);

  /// Used to test if there is data loaded (i.e. not an empty session)
  bool hasDataSources();
  bool hasMoleculeSources();

  /// Used when loading a model.  If there are additional child pipelines that
  /// need to finish processing before stateDoneLoading is emitted, then this
  /// must be called for each of them so the module manager knows how many
  /// pipelineFinsihed signals to wait for.
  void incrementPipelinesToWaitFor();

  bool lastLoadStateSucceeded();

  void executePipelinesOnLoad(bool execute);
  bool executePipelinesOnLoad() const;
  DataSource* loadDataSource(QJsonObject& ds);

  /// Used to keep track of the most recent state file
  void setMostRecentStateFile(const QString& s);
  QString mostRecentStateFile() const { return m_mostRecentStateFile; }

  // Keep a record of state ids to data sources
  void addStateIdToDataSource(QString id, DataSource* dataSource)
  {
    m_stateIdToDataSource[id] = dataSource;
  }

  DataSource* dataSourceForStateId(QString id)
  {
    return m_stateIdToDataSource.value(id, nullptr);
  }

public slots:
  void addModule(Module*);

  /// Use these methods to delete/remove modules.
  void removeModule(Module*);
  void removeAllModules();
  void removeAllModules(DataSource* source);

  /// Creates and add a new module.
  Module* createAndAddModule(const QString& type, DataSource* dataSource,
                             vtkSMViewProxy* view);
  Module* createAndAddModule(const QString& type, MoleculeSource* dataSource,
                             vtkSMViewProxy* view);
  Module* createAndAddModule(const QString& type, OperatorResult* result,
                             vtkSMViewProxy* view);

  /// Register/Unregister data sources with the ModuleManager.
  void addDataSource(DataSource*);
  void addMoleculeSource(MoleculeSource*);
  void addChildDataSource(DataSource*);
  void removeDataSource(DataSource*);
  void removeMoleculeSource(MoleculeSource*);
  void removeChildDataSource(DataSource*);
  void removeAllDataSources();
  void removeAllMoleculeSources();
  void removeOperator(Operator*);

  /// Removes all modules and data sources.
  void reset();

private slots:
  /// Used when loading state
  void onPVStateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*);

  /// Delete modules when the view that they are in is removed.
  void onViewRemoved(pqView*);

  void render();
  void updateClientSideView();

  void onPipelineFinished();

signals:
  void moduleAdded(Module*);
  void moduleRemoved(Module*);

  void dataSourceAdded(DataSource*);
  void childDataSourceAdded(DataSource*);
  void dataSourceRemoved(DataSource*);
  void moleculeSourceRemoved(MoleculeSource*);
  void childDataSourceRemoved(DataSource*);

  void moleculeSourceAdded(MoleculeSource*);

  void operatorRemoved(Operator*);

  void stateDoneLoading();

  void enablePythonConsole(bool enable);

  void mostRecentStateFileChanged(const QString& s);

  void visibilityChanged(bool);

  void mouseOverVoxel(const vtkVector3i& ijk, double v);

  void pipelineViewRenderNeeded();

private:
  Q_DISABLE_COPY(ModuleManager)
  ModuleManager(QObject* parent = nullptr);
  ~ModuleManager();

  void loadDataSources(const QJsonArray& dataSources);

  class MMInternals;
  QScopedPointer<MMInternals> d;

  QMap<QString, DataSource*> m_stateIdToDataSource;
  QString m_mostRecentStateFile = "";
  QJsonObject m_stateObject;
  bool m_loadDataSources = true;
  bool m_isDeserializing = false;
};
} // namespace tomviz

#endif
