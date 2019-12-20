/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleFactory_h
#define tomvizModuleFactory_h

#include <QIcon>
#include <QObject>

class vtkSMViewProxy;

namespace tomviz {
class DataSource;
class MoleculeSource;
class Module;
class OperatorResult;

class ModuleFactory
{
  typedef QObject Superclass;

public:
  /// Returns a list of module types that can be created for the data source
  /// in the provided view.
  static QList<QString> moduleTypes();

  /// Returns whether the module of the given name is applicable to the
  /// DataSource and View.
  static bool moduleApplicable(const QString& moduleName,
                               DataSource* dataSource, vtkSMViewProxy* view);
  static bool moduleApplicable(const QString& moduleName,
                               MoleculeSource* moleculeSource,
                               vtkSMViewProxy* view);

  /// Creates a module of the given type to show the dataSource in the view.
  static Module* createModule(const QString& type, DataSource* dataSource,
                              vtkSMViewProxy* view);
  static Module* createModule(const QString& type,
                              MoleculeSource* moleculeSource,
                              vtkSMViewProxy* view);
  static Module* createModule(const QString& type, OperatorResult* result,
                              vtkSMViewProxy* view);

  /// Returns the type for a module instance.
  static const char* moduleType(const Module* module);

  /// Returns the icon for a module.
  static QIcon moduleIcon(const QString& type);

private:
  ModuleFactory();
  ~ModuleFactory();
  Q_DISABLE_COPY(ModuleFactory)
  static Module* allocateModule(const QString& type);
};
} // namespace tomviz

#endif
