/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCustomPythonOperatorWidget_h
#define tomvizCustomPythonOperatorWidget_h

#include <QWidget>

namespace tomviz {

class CustomPythonOperatorWidget : public QWidget
{
  Q_OBJECT

public:
  CustomPythonOperatorWidget(QWidget* parent);
  virtual ~CustomPythonOperatorWidget();

  virtual void getValues(QMap<QString, QVariant>& map) = 0;
  virtual void setValues(const QMap<QString, QVariant>& map) = 0;
};
} // namespace tomviz

#endif
