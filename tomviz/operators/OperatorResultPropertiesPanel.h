/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorResultPropertiesPanel_h
#define tomvizOperatorResultPropertiesPanel_h

#include <QWidget>

#include <QPointer>

class QLabel;
class QTableWidget;
class QVBoxLayout;
class vtkMolecule;

namespace tomviz {
class OperatorResult;

class OperatorResultPropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  OperatorResultPropertiesPanel(QWidget* parent = nullptr);
  virtual ~OperatorResultPropertiesPanel();

private slots:
  void setOperatorResult(OperatorResult*);

private:
  Q_DISABLE_COPY(OperatorResultPropertiesPanel)

  QPointer<OperatorResult> m_activeOperatorResult = nullptr;
  QVBoxLayout* m_layout = nullptr;
};
} // namespace tomviz

#endif
