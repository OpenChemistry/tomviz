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
#ifndef tomvizModuleManager_h
#define tomvizModuleManager_h

#include <QObject>

#include "Module.h"

#include <QJsonObject>
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

public slots:
  void addModule(Module*);

  /// Use these methods to delete/remove modules.
  void removeModule(Module*);
  void removeAllModules();
  void removeAllModules(DataSource* source);

  /// Creates and add a new module.
  Module* createAndAddModule(const QString& type, DataSource* dataSource,
                             vtkSMViewProxy* view);

  /// Register/Unregister data sources with the ModuleManager.
  void addDataSource(DataSource*);
  void addChildDataSource(DataSource*);
  void removeDataSource(DataSource*);
  void removeChildDataSource(DataSource*);
  void removeAllDataSources();
  void removeOperator(Operator*);

  /// Removes all modules and data sources.
  void reset();

private slots:
  /// Used when loading state
  void onPVStateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*);

  /// Delete modules when the view that they are in is removed.
  void onViewRemoved(pqView*);

  void render();

signals:
  void moduleAdded(Module*);
  void moduleRemoved(Module*);

  void dataSourceAdded(DataSource*);
  void childDataSourceAdded(DataSource*);
  void dataSourceRemoved(DataSource*);
  void childDataSourceRemoved(DataSource*);

  void operatorRemoved(Operator*);

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
