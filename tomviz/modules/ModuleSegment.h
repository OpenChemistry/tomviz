/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleSegment_h
#define tomvizModuleSegment_h

#include "Module.h"

#include <QScopedPointer>

namespace tomviz {

class ModuleSegment : public Module
{
  Q_OBJECT

public:
  ModuleSegment(QObject* parent = nullptr);
  ~ModuleSegment();

  /// Returns a  label for this module.
  QString label() const override;

  /// Returns an icon to use for this module.
  QIcon icon() const override;

  using Module::initialize;
  /// Initialize the module for the data source and view. This is called after a
  /// new module is instantiated. Subclasses override this method to setup the
  /// visualization pipeline for this module.
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;

  /// Finalize the module. Subclasses should override this method to delete and
  /// release all proxies (and data) created for this module.
  bool finalize() override;

  /// Returns the visibility for the module.
  bool visibility() const override;

  /// Set the visibility for this module. Subclasses should override this method
  /// show/hide all representations created for this module.
  bool setVisibility(bool val) override;

  /// This method is called add the proxies in this module to a
  /// pqProxiesWidget instance. Default implementation simply adds the view
  /// properties. Subclasses should override to add proxies and relevant
  /// properties to the panel.
  void addToPanel(QWidget* panel) override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

protected:
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private slots:
  void onPropertyChanged();

private:
  void updateColorMap() override;

  class MSInternal;
  QScopedPointer<MSInternal> d;
};
} // namespace tomviz

#endif
