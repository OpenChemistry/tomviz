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
#ifndef tomvizModuleFactory_h
#define tomvizModuleFactory_h

#include <QIcon>
#include <QObject>

class vtkSMViewProxy;

namespace tomviz {
class DataSource;
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

  /// Creates a module of the given type to show the dataSource in the view.
  static Module* createModule(const QString& type, DataSource* dataSource,
                              vtkSMViewProxy* view, OperatorResult* result = nullptr);

  /// Returns the type for a module instance.
  static const char* moduleType(Module* module);

  /// Returns the icon for a module.
  static QIcon moduleIcon(const QString& type);

private:
  ModuleFactory();
  ~ModuleFactory();
  Q_DISABLE_COPY(ModuleFactory)
};
} // namespace tomviz

#endif
