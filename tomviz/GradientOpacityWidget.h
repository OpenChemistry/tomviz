/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizGradientOpacityWidget_h
#define tomvizGradientOpacityWidget_h

#include <QWidget>

#include <vtkNew.h>
#include <vtkSmartPointer.h>

/**
 * \brief Similar to HistogramWidget but keeps everything client side
 * (no proxy infrastructure is used). Displays a 1D gradient opacity
 * function.
 */

class vtkChartGradientOpacityEditor;
class vtkContextView;
class vtkEventQtSlotConnect;
class vtkPiecewiseFunction;
class vtkObject;
class vtkSMProxy;
class vtkTable;

namespace tomviz {

class QVTKGLWidget;

class GradientOpacityWidget : public QWidget
{
  Q_OBJECT

public:
  explicit GradientOpacityWidget(QWidget* parent_ = nullptr);
  ~GradientOpacityWidget() override;

  /**
   * The proxy is only required to set the range. The actual opacity
   * function for this widget is defined by gradientOpacity.
   */
  void setLUT(vtkPiecewiseFunction* gradientOpacity);

  virtual void setInputData(vtkTable* table, const char* x_, const char* y_);

signals:
  void mapUpdated();

public slots:
  virtual void onOpacityFunctionChanged();

protected:
  vtkNew<vtkChartGradientOpacityEditor> m_histogramColorOpacityEditor;
  vtkNew<vtkContextView> m_histogramView;
  vtkPiecewiseFunction* m_scalarOpacityFunction = nullptr;
  vtkNew<vtkEventQtSlotConnect> m_eventLink;

private:
  void renderViews();

  /**
   * For gradient magnitude, the volume mapper's fragment shader expects a
   * range of [0, DataMax/4].  As the gradient magnitude histogram is currently
   * not being computed, a dummy table is created here just to adjust the range.
   * This will change once the actual histogram is computed.
   */
  void prepareAdjustedTable(vtkTable* table, const char* x_);

  QVTKGLWidget* m_qvtk;

  vtkSmartPointer<vtkTable> m_adjustedTable;
};
} // namespace tomviz
#endif // tomvizGradientOpacityWidget_h
