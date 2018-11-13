/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMoleculeProperties_h
#define tomvizMoleculeProperties_h

#include <QWidget>

class QTableWidget;
class vtkMolecule;

namespace tomviz {
class MoleculeProperties : public QWidget
{
  Q_OBJECT

public:
  MoleculeProperties(vtkMolecule* molecule, QWidget* parent = nullptr);
  // virtual ~MoleculeProperties();
private:
  QTableWidget* initializeAtomTable();
  void populateAtomTable(QTableWidget* table, vtkMolecule* molecule);
  QMap<QString, int> moleculeSpeciesCount(vtkMolecule* molecule);
};
}

#endif
