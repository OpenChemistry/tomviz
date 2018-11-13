/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorPropertiesPanel_h
#define tomvizOperatorPropertiesPanel_h

#include <QWidget>

#include <QPointer>

class QLabel;
class QVBoxLayout;

namespace tomviz {
class Operator;
class OperatorPython;
class OperatorWidget;

class OperatorPropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  OperatorPropertiesPanel(QWidget* parent = nullptr);
  virtual ~OperatorPropertiesPanel();

private slots:
  void setOperator(Operator*);
  void setOperator(OperatorPython*);
  void apply();
  void viewCodePressed();

private:
  Q_DISABLE_COPY(OperatorPropertiesPanel)

  QPointer<Operator> m_activeOperator = nullptr;
  QVBoxLayout* m_layout = nullptr;
  OperatorWidget* m_operatorWidget = nullptr;
};
} // namespace tomviz

#endif
