/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizManualManipulationWidget_h
#define tomvizManualManipulationWidget_h

#include "CustomPythonOperatorWidget.h"

#include "vtkSmartPointer.h"

#include <QScopedPointer>

class vtkImageData;

namespace tomviz {
class Operator;

class ManualManipulationWidget : public CustomPythonOperatorWidget
{
  Q_OBJECT

public:
  ManualManipulationWidget(Operator* op, vtkSmartPointer<vtkImageData> image,
                           QWidget* parent = nullptr);
  ~ManualManipulationWidget();

  static CustomPythonOperatorWidget* New(QWidget* p, Operator* op,
                                         vtkSmartPointer<vtkImageData> data);

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

private:
  Q_DISABLE_COPY(ManualManipulationWidget)

  class Internal;
  QScopedPointer<Internal> m_internal;
};
} // namespace tomviz
#endif
