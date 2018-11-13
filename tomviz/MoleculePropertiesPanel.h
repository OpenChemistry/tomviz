/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMoleculePropertiesPanel_h
#define tomvizMoleculePropertiesPanel_h

#include <QWidget>

#include <QPointer>

class QVBoxLayout;
class QLineEdit;

namespace tomviz {

class MoleculeSource;
class MoleculeProperties;

class MoleculePropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  explicit MoleculePropertiesPanel(QWidget* parent = nullptr);
  ~MoleculePropertiesPanel() override;

private slots:
  void setMoleculeSource(MoleculeSource*);

private:
  Q_DISABLE_COPY(MoleculePropertiesPanel)

  void update();

  QPointer<MoleculeSource> m_currentMoleculeSource;
  QVBoxLayout* m_layout;
  QLineEdit* m_label;
  MoleculeProperties* m_moleculeProperties = nullptr;
};
}

#endif
