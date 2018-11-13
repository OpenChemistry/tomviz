/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorWidget_h
#define tomvizOperatorWidget_h

#include <OperatorPython.h>

#include <QMap>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace tomviz {

class InterfaceBuilder;

class OperatorWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  OperatorWidget(QWidget* parent = nullptr);
  ~OperatorWidget() override;

  void setupUI(const QString& json);
  void setupUI(OperatorPython* op);

  /// Get parameter values
  QMap<QString, QVariant> values() const;

private:
  Q_DISABLE_COPY(OperatorWidget)
  OperatorPython* m_operator = nullptr;
  void buildInterface(InterfaceBuilder* builder);
};
} // namespace tomviz

#endif
