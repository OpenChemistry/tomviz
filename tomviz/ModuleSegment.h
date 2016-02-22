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
  ModuleSegment(QObject *parent = nullptr);
  ~ModuleSegment();

  /// Returns a  label for this module.
  QString label() const override;

  /// Returns an icon to use for this module.
  QIcon icon() const override;

  /// Initialize the module for the data source and view. This is called after a
  /// new module is instantiated. Subclasses override this method to setup the
  /// visualization pipeline for this module.
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;

  /// Finalize the module. Subclasses should override this method to delete and
  /// release all proxies (and data) created for this module.
  bool finalize() override;

  /// Returns the visibility for the module.
  bool visibility() const override;

  /// serialize the state of the module.
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  /// Set the visibility for this module. Subclasses should override this method
  /// show/hide all representations created for this module.
  bool setVisibility(bool val) override;

  /// This method is called add the proxies in this module to a
  /// pqProxiesWidget instance. Default implementation simply adds the view
  /// properties.
  /// Subclasses should override to add proxies and relevant properties to the
  /// panel.
  void addToPanel(pqProxiesWidget* panel) override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

private slots:
  void onPropertyChanged();
private:

  void updateColorMap() override;

  class MSInternal;
  QScopedPointer<MSInternal> Internals;
};

}

#endif
