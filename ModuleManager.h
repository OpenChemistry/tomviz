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
#ifndef __ModuleManager_h
#define __ModuleManager_h

#include <QObject>
#include <QScopedPointer>

namespace TEM
{
  class Module;

  /// Singleton akin to ProxyManager, but to keep track (and
  /// serialize/deserialze) modules.
  class ModuleManager : public QObject
  {
  Q_OBJECT;
  typedef QObject Superclass;
public:
  static ModuleManager& instance();

public slots:
  void addModule(Module*);
  void removeModule(Module*);
  void removeAllModules();

signals:
  void moduleAdded(Module*);
  void moduleRemoved(Module*);

private:
  Q_DISABLE_COPY(ModuleManager);
  ModuleManager(QObject* parent=NULL);
  ~ModuleManager();

  class MMInternals;
  QScopedPointer<MMInternals> Internals;
  };
}
#endif
