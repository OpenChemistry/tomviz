/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizFxiWorkflowWidget_h
#define tomvizFxiWorkflowWidget_h

#include "CustomPythonOperatorWidget.h"

#include <vtkSmartPointer.h>

#include <QScopedPointer>

class vtkImageData;

namespace tomviz {
class Operator;

class FxiWorkflowWidget : public CustomPythonOperatorWidget
{
  Q_OBJECT
  typedef CustomPythonOperatorWidget Superclass;

public:
  FxiWorkflowWidget(Operator* op, vtkSmartPointer<vtkImageData> image,
                    QWidget* parent = NULL);
  ~FxiWorkflowWidget();

  static CustomPythonOperatorWidget* New(QWidget* p, Operator* op,
                                         vtkSmartPointer<vtkImageData> data);

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

  void setScript(const QString& script) override;
  void setupUI(OperatorPython* op) override;

private:
  Q_DISABLE_COPY(FxiWorkflowWidget)

  class Internal;
  QScopedPointer<Internal> m_internal;
};

inline CustomPythonOperatorWidget* FxiWorkflowWidget::New(
  QWidget* p, Operator* op, vtkSmartPointer<vtkImageData> data)
{
  return new FxiWorkflowWidget(op, data, p);
}

} // namespace tomviz
#endif
