/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizShiftRotationCenterWidget_h
#define tomvizShiftRotationCenterWidget_h

#include "CustomPythonOperatorWidget.h"

#include <vtkSmartPointer.h>

#include <QScopedPointer>

class vtkImageData;

namespace tomviz {
class Operator;

class ShiftRotationCenterWidget : public CustomPythonOperatorWidget
{
  Q_OBJECT
  typedef CustomPythonOperatorWidget Superclass;

public:
  ShiftRotationCenterWidget(Operator* op, vtkSmartPointer<vtkImageData> image,
                            QWidget* parent = NULL);
  ~ShiftRotationCenterWidget();

  static CustomPythonOperatorWidget* New(QWidget* p, Operator* op,
                                         vtkSmartPointer<vtkImageData> data);

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

  void setScript(const QString& script) override;

  void writeSettings() override;

private:
  Q_DISABLE_COPY(ShiftRotationCenterWidget)

  class Internal;
  QScopedPointer<Internal> m_internal;
};

inline CustomPythonOperatorWidget* ShiftRotationCenterWidget::New(
  QWidget* p, Operator* op, vtkSmartPointer<vtkImageData> data)
{
  return new ShiftRotationCenterWidget(op, data, p);
}

} // namespace tomviz
#endif
