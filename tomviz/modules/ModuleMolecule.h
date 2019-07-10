/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleMolecule_h
#define tomvizModuleMolecule_h

#include "Module.h"

class QCheckBox;

class vtkActor;
class vtkMolecule;
class vtkMoleculeMapper;

class vtkPVRenderView;

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
  vtkDataObject* dataToExport() override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

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
