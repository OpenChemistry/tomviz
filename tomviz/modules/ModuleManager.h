/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleManager_h
#define tomvizModuleManager_h

#include <QObject>

#include "Module.h"

#include <QJsonObject>
#include <QScopedPointer>

class pqView;
class vtkPlane;
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

  QList<Module*> findModulesGeneric(const DataSource* dataSource,
                                    const vtkSMViewProxy* view);

  QList<Module*> findModulesGeneric(const MoleculeSource* dataSource,
                                    const vtkSMViewProxy* view);

  /// Save the application state as JSON, use stateDir as the base for relative
  /// paths.
  bool serialize(QJsonObject& doc, const QDir& stateDir,
                 bool interative = true) const;
  bool deserialize(const QJsonObject& doc, const QDir& stateDir);

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

  void onPipelineFinished();

  void clip(vtkPlane* plane, bool newFilter);

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

  void clipChanged(vtkPlane* plane, bool newFilter);
  void enablePythonConsole(bool enable);

private:
  Q_DISABLE_COPY(ModuleManager)
  ModuleManager(QObject* parent = nullptr);
  ~ModuleManager();

  class MMInternals;
  QScopedPointer<MMInternals> d;

  QJsonObject m_stateObject;
};
} // namespace tomviz

#endif
