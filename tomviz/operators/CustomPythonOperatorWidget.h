/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCustomPythonOperatorWidget_h
#define tomvizCustomPythonOperatorWidget_h

#include <QWidget>

namespace tomviz {

class OperatorPython;

class CustomPythonOperatorWidget : public QWidget
{
  Q_OBJECT

public:
  CustomPythonOperatorWidget(QWidget* parent);
  virtual ~CustomPythonOperatorWidget();

  virtual void getValues(QMap<QString, QVariant>& map) = 0;
  virtual void setValues(const QMap<QString, QVariant>& map) = 0;

  // Keep a copy of the current script (including edits) in case the
  // custom python operator needs to use it.
  virtual void setScript(const QString& script) { m_script = script; }

  // Subclasses can perform some UI setup when this is called, if needed
  virtual void setupUI(OperatorPython*) {}

protected:
  QString m_script;
};
} // namespace tomviz

#endif
