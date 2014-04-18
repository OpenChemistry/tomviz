/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __ModuleFactory_h
#define __ModuleFactory_h

#include <QObject>

class vtkSMSourceProxy;
class vtkSMViewProxy;

namespace TEM
{

class Module;

class ModuleFactory
{
  typedef QObject Superclass;
public:
  /// Returns a list of module types that can be created for the data source
  /// in the provided view.
  static QList<QString> moduleTypes(
    vtkSMSourceProxy* dataSource, vtkSMViewProxy* view);

  /// Creates a module of the given type to show the dataSource in the view.
  static Module* createModule(const QString& type, vtkSMSourceProxy* dataSource,
                              vtkSMViewProxy* view);

private:
  ModuleFactory();
  ~ModuleFactory();
  Q_DISABLE_COPY(ModuleFactory)
};

}

#endif
