/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleMolecule_h
#define tomvizModuleMolecule_h

#include "Module.h"

#include <vtkActor.h>
#include <vtkMoleculeMapper.h>
#include <vtkPVRenderView.h>
#include <vtkWeakPointer.h>

class QCheckBox;
class vtkMolecule;
class vtkSMProxy;
class vtkSMSourceProxy;

namespace tomviz {

class MoleculeSource;
class OperatorResult;

class ModuleMolecule : public Module
{
  Q_OBJECT

public:
  ModuleMolecule(QObject* parent = nullptr);
  ~ModuleMolecule() override;

  QString label() const override { return "Molecule"; }
  QIcon icon() const override;
  using Module::initialize;
  bool initialize(MoleculeSource* moleculeSource,
                  vtkSMViewProxy* view) override;
  bool initialize(OperatorResult* result, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  void addToPanel(QWidget*) override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QString exportDataTypeString() override { return "Molecule"; }
  vtkSmartPointer<vtkDataObject> getDataToExport() override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;
  void dataSourceMoved(double newX, double newY, double newZ) override;

protected:
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private slots:
  void ballRadiusChanged(double val);
  void bondRadiusChanged(double val);

private:
  Q_DISABLE_COPY(ModuleMolecule)
  void addMoleculeToView(vtkSMViewProxy* view);
  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkMolecule* m_molecule;
  vtkNew<vtkMoleculeMapper> m_moleculeMapper;
  vtkNew<vtkActor> m_moleculeActor;
};
} // namespace tomviz
#endif
