/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorDialog_h
#define tomvizOperatorDialog_h

#include <QDialog>
#include <QMap>
#include <QString>
#include <QVariant>

namespace tomviz {

class OperatorWidget;

class OperatorDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;

public:
  OperatorDialog(QWidget* parent = nullptr);
  ~OperatorDialog() override;

  /// Set the JSON description of the operator
  void setJSONDescription(const QString& json);

  /// Get parameter values
  QMap<QString, QVariant> values() const;

private:
  Q_DISABLE_COPY(OperatorDialog)
  OperatorWidget* m_ui = nullptr;
};
} // namespace tomviz

#endif
