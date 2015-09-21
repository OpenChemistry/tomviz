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
#ifndef tomvizModuleSegment_h
#define tomvizModuleSegment_h

#include "Module.h"

#include <QScopedPointer>

namespace tomviz
{

class ModuleSegment : public Module
{
  Q_OBJECT
  typedef Module Superclass;
public:
  ModuleSegment(QObject *parent = NULL);
  ~ModuleSegment();

  /// Returns a  label for this module.
  virtual QString label() const;

  /// Returns an icon to use for this module.
  virtual QIcon icon() const;

  /// Initialize the module for the data source and view. This is called after a
  /// new module is instantiated. Subclasses override this method to setup the
  /// visualization pipeline for this module.
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view);

  /// Finalize the module. Subclasses should override this method to delete and
  /// release all proxies (and data) created for this module.
  virtual bool finalize();

  /// Returns the visibility for the module.
  virtual bool visibility() const;

  /// serialize the state of the module.
  virtual bool serialize(pugi::xml_node& ns) const;
  virtual bool deserialize(const pugi::xml_node& ns);

  /// Set the visibility for this module. Subclasses should override this method
  /// show/hide all representations created for this module.
  virtual bool setVisibility(bool val);

  /// This method is called add the proxies in this module to a
  /// pqProxiesWidget instance. Default implementation simply adds the view
  /// properties.
  /// Subclasses should override to add proxies and relevant properties to the
  /// panel.
  virtual void addToPanel(pqProxiesWidget* panel);

private slots:
  void onPropertyChanged();
private:

  void updateColorMap();

  class MSInternal;
  QScopedPointer<MSInternal> Internals;
};

}

#endif
