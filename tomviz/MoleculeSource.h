/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMoleculeSource_h
#define tomvizMoleculeSource_h

#include <QObject>

#include <QJsonObject>

#include <vtkSmartPointer.h>

class vtkMolecule;

namespace tomviz {
class Pipeline;

class MoleculeSource : public QObject
{
  Q_OBJECT

public:
  MoleculeSource(vtkMolecule* molecule, QObject* parent = nullptr);
  ~MoleculeSource() override;

  /// Save the state out.
  QJsonObject serialize() const;
  bool deserialize(const QJsonObject& state);

  /// Set the file name.
  void setFileName(QString label);

  /// Returns the file used to load the atoms.
  QString fileName() const;

  /// Set the label for the data source.
  void setLabel(const QString& label);

  /// Returns the name of the filename used from the originalDataSource.
  QString label() const;

  /// Returns a pointer to the vtkMolecule
  vtkMolecule* molecule() const;

private:
  QJsonObject m_json;
  vtkSmartPointer<vtkMolecule> m_molecule;
};

} // namespace tomviz

#endif
