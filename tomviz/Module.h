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
#ifndef __Module_h
#define __Module_h

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QScopedPointer>
#include "vtkWeakPointer.h"
#include <vtk_pugixml.h>

class pqProxiesWidget;
class vtkSMProxy;
class vtkSMViewProxy;

namespace TEM
{
class DataSource;
/// Abstract parent class for all Modules in TomViz.
class Module : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  Module(QObject* parent=NULL);
  virtual ~Module();

  /// Returns a  label for this module.
  virtual QString label() const = 0;

  /// Returns an icon to use for this module.
  virtual QIcon icon() const=0;

  /// Initialize the module for the data source and view. This is called after a
  /// new module is instantiated. Subclasses override this method to setup the
  /// visualization pipeline for this module.
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view);

  /// Finalize the module. Subclasses should override this method to delete and
  /// release all proxies (and data) created for this module.
  virtual bool finalize()=0;

  /// Returns the visibility for the module.
  virtual bool visibility() const =0;

  /// Accessors for the data-source and view associated with this Plot.
  DataSource* dataSource() const;
  vtkSMViewProxy* view() const;

  /// serialize the state of the module.
  virtual bool serialize(pugi::xml_node& ns) const;
  virtual bool deserialize(const pugi::xml_node& ns);

  /// Modules that use transfer functions should override this method to return
  /// true.
  virtual bool isColorMapNeeded() const { return false; }

  /// Flag indicating whether the module uses a "detached" color map or not.
  /// This is only applicable when isColorMapNeeded() return true.
  void setUseDetachedColorMap(bool);
  bool useDetachedColorMap() const { return this->UseDetachedColorMap; }

public slots:
  /// Set the visibility for this module. Subclasses should override this method
  /// show/hide all representations created for this module.
  virtual bool setVisibility(bool val)=0;

  bool show() { return this->setVisibility(true); }
  bool hide() { return this->setVisibility(false); }

  /// This method is called add the proxies in this module to a
  /// pqProxiesWidget instance. Default implementation simply adds the view
  /// properties.
  /// Subclasses should override to add proxies and relevant properties to the
  /// panel.
  virtual void addToPanel(pqProxiesWidget* panel);

protected:
  /// Modules that use transfer functions for color/opacity should override this
  /// method to set the color map on appropriate representations. This will be
  /// called when the color map proxy is changed, for example, when
  /// setUseDetachedColorMap is toggled.
  virtual void updateColorMap() {}

  /// Subclasses can use this method to get the current color/opacity maps.
  /// This will either return the maps from the data source or detached ones
  /// based on the UseDetachedColorMap flag.
  vtkSMProxy* colorMap() const;
  vtkSMProxy* opacityMap() const;

private:
  Q_DISABLE_COPY(Module)
  QPointer<DataSource> ADataSource;
  vtkWeakPointer<vtkSMViewProxy> View;
  bool UseDetachedColorMap;

  class MInternals;
  const QScopedPointer<MInternals> Internals;
};

}
#endif
