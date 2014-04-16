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
#include "pqActiveObjects.h" // needed for pqActiveObjects.

class pqView;
class pqPipelineSource;
class vtkSMSessionProxyManager;

namespace TEM
{
  /// ActiveObjects keeps track of active objects in MatViz.
  /// This is similar to pqActiveObjects in ParaView, however tracks objects
  /// relevant to MatViz.
  class ActiveObjects : public QObject
  {
  Q_OBJECT;
  typedef QObject Superclass;
public:
  /// Returns reference to the singleton instance.
  static ActiveObjects& instance();

  /// Returns the active view.
  pqView* activeView() const
    { return pqActiveObjects::instance().activeView(); }

  /// Returns the active data source.
  pqPipelineSource* activeDataSource() const
    { return this->ActiveDataSource; }

  /// Returns the vtkSMSessionProxyManager from the active server/session.
  /// Provided here for convenience, since we need to access the proxy manager
  /// often.
  vtkSMSessionProxyManager* proxyManager() const;

public slots:
  /// Set the active view;
  void setActiveView(pqView*);

  /// Set the active data source.
  void setActiveDataSource(pqPipelineSource* source);

signals:
  /// fired whenever the active view changes.
  void viewChanged(pqView*);

  /// fired whenever the active data source changes.
  void dataSourceChanged(pqPipelineSource*);

protected:
  ActiveObjects();
  virtual ~ActiveObjects();

  QPointer<pqPipelineSource> ActiveDataSource;
  void* VoidActiveDataSource;

private:
  Q_DISABLE_COPY(ActiveObjects);
  };
}
#endif
