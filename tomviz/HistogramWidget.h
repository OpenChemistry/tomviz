/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizHistogramWidget_h
#define tomvizHistogramWidget_h

#include <QWidget>

#include <vtkNew.h>
#include <vtkWeakPointer.h>

class vtkChartHistogramColorOpacityEditor;
class vtkContextView;
class vtkEventQtSlotConnect;
class vtkPiecewiseFunction;
class vtkObject;
class vtkTable;

class QToolButton;

class vtkPVDiscretizableColorTransferFunction;
class vtkSMProxy;

namespace tomviz {

class PresetDialog;
class QVTKGLWidget;

class HistogramWidget : public QWidget
{
  Q_OBJECT

public:
  explicit HistogramWidget(QWidget* parent_ = nullptr);
  ~HistogramWidget() override;

  void setLUT(vtkPVDiscretizableColorTransferFunction* lut);
  void setLUTProxy(vtkSMProxy* proxy);

  void setInputData(vtkTable* table, const char* x_, const char* y_);

  vtkSMProxy* getScalarBarRepresentation(vtkSMProxy* view);

signals:
  void colorMapUpdated();
  void opacityChanged();
  void colorLegendToggled(bool);

public slots:
  void onScalarOpacityFunctionChanged();
  void onCurrentPointEditEvent();
  void histogramClicked(vtkObject*);

  void onResetRangeClicked();
  void onCustomRangeClicked();
  void onInvertClicked();
  void onPresetClicked();
  void onSaveToPresetClicked();
  void applyCurrentPreset();
  void updateUI();

protected:
  void showEvent(QShowEvent* event) override;

private:
  void renderViews();
  void rescaleTransferFunction(vtkSMProxy* lutProxy, double min, double max);
  bool createContourDialog(double& isoValue);
  void showPresetDialog(const char* presetName);
  vtkNew<vtkChartHistogramColorOpacityEditor> m_histogramColorOpacityEditor;
  vtkNew<vtkContextView> m_histogramView;
  vtkNew<vtkEventQtSlotConnect> m_eventLink;
  QToolButton* m_colorLegendToolButton;

  vtkWeakPointer<vtkPVDiscretizableColorTransferFunction> m_LUT;
  vtkWeakPointer<vtkPiecewiseFunction> m_scalarOpacityFunction;
  vtkWeakPointer<vtkSMProxy> m_LUTProxy;
  vtkWeakPointer<vtkTable> m_inputData;

  PresetDialog* m_presetDialog = nullptr;
  QVTKGLWidget* m_qvtk;
};
} // namespace tomviz

#endif // tomvizHistogramWidget_h
