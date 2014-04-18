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
#ifndef __ActiveObjects_h
#define __ActiveObjects_h

#include <QObject>
#include <QPointer>
#include "vtkWeakPointer.h" // needed for vtkWeakPointer.
#include "vtkSMSourceProxy.h"
#include "Module.h"

class pqView;
class vtkSMSessionProxyManager;
class vtkSMSourceProxy;
class vtkSMViewProxy;

namespace TEM
{
class Module;

/// ActiveObjects keeps track of active objects in MatViz.
/// This is similar to pqActiveObjects in ParaView, however tracks objects
/// relevant to MatViz.
class ActiveObjects : public QObject
  {
  Q_OBJECT

  typedef QObject Superclass;

public:
  /// Returns reference to the singleton instance.
  static ActiveObjects& instance();

  /// Returns the active view.
  vtkSMViewProxy* activeView() const;

  /// Returns the active data source.
  vtkSMSourceProxy* activeDataSource() const
    { return this->ActiveDataSource; }

  /// Returns the active module.
  Module* activeModule() const
    { return this->ActiveModule; }

  /// Returns the vtkSMSessionProxyManager from the active server/session.
  /// Provided here for convenience, since we need to access the proxy manager
  /// often.
  vtkSMSessionProxyManager* proxyManager() const;

public slots:
  /// Set the active view;
  void setActiveView(vtkSMViewProxy*);

  /// Set the active data source.
  void setActiveDataSource(vtkSMSourceProxy* source);

  /// Set the active module.
  void setActiveModule(Module* module);

signals:
  /// fired whenever the active view changes.
  void viewChanged(vtkSMViewProxy*);

  /// fired whenever the active data source changes.
  void dataSourceChanged(vtkSMSourceProxy*);

  /// fired whenever the active module changes.
  void moduleChanged(Module*);

private slots:
  void viewChanged(pqView*);

protected:
  ActiveObjects();
  virtual ~ActiveObjects();

  vtkWeakPointer<vtkSMSourceProxy> ActiveDataSource;
  void* VoidActiveDataSource;

  QPointer<Module> ActiveModule;
  void* VoidActiveModule;
private:
  Q_DISABLE_COPY(ActiveObjects)
};
}
#endif
