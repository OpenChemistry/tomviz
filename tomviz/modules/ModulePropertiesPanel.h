/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModulePropertiesPanel_h
#define tomvizModulePropertiesPanel_h

#include <QScopedPointer>
#include <QWidget>

class vtkSMViewProxy;

namespace tomviz {
class Module;

class ModulePropertiesPanel : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  ModulePropertiesPanel(QWidget* parent = nullptr);
  virtual ~ModulePropertiesPanel();

private slots:
  void setModule(Module*);
  void setView(vtkSMViewProxy*);
  void updatePanel();
  void detachColorMap(bool);
  void onEnforcedOpacity(bool);

private:
  Q_DISABLE_COPY(ModulePropertiesPanel)

  class MPPInternals;
  const QScopedPointer<MPPInternals> Internals;
};
} // namespace tomviz

#endif
