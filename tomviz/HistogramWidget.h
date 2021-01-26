/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizHistogramWidget_h
#define tomvizHistogramWidget_h

#include <QScopedPointer>
#include <QWidget>

#include <vtkNew.h>
#include <vtkWeakPointer.h>

class vtkChartHistogramColorOpacityEditor;
class vtkContextView;
class vtkDataArray;
class vtkEventQtSlotConnect;
class vtkImageData;
class vtkPiecewiseFunction;
class vtkObject;
class vtkTable;

class QDialog;
class QToolButton;

class vtkDiscretizableColorTransferFunction;
class vtkSMProxy;

namespace tomviz {

class ColorMapSettingsWidget;
class PresetDialog;
class QVTKGLWidget;

class HistogramWidget : public QWidget
{
  Q_OBJECT

public:
  explicit HistogramWidget(QWidget* parent_ = nullptr);
  ~HistogramWidget() override;

  void setLUT(vtkDiscretizableColorTransferFunction* lut);
  void setLUTProxy(vtkSMProxy* proxy);

  void setInputData(vtkTable* table, const char* x_, const char* y_);

  vtkSMProxy* getScalarBarRepresentation(vtkSMProxy* view);

signals:
  void colorMapUpdated();
  void opacityChanged();
  void colorLegendToggled(bool);

public slots:
  void onColorFunctionChanged();
  void onScalarOpacityFunctionChanged();
  void onCurrentPointEditEvent();
  void histogramClicked(vtkObject*);

  void onResetRangeClicked();
  void onCustomRangeClicked();
  void onInvertClicked();
  void onColorMapSettingsClicked();
  void onPresetClicked();
  void onSaveToPresetClicked();
  void onAutoAdjustContrastClicked();
  void applyCurrentPreset();
  void updateUI();

protected:
  void showEvent(QShowEvent* event) override;

private:
  void setupColorMapSettingsDialog();

  void renderViews();
  void rescaleTransferFunction(vtkSMProxy* lutProxy, double min, double max);
  bool createContourDialog(double& isoValue);
  void showPresetDialog(const QJsonObject& newPreset);

  void resetRange();
  void resetRange(double range[2]);

  // When the user modifies the LUT via the control points, we unfortunately
  // need to update the proxy with these modifications, or any operations
  // with the proxy will over-write what the user has done.
  void updateLUTProxy();

  // Auto contrast functions
  void autoAdjustContrast();
  void autoAdjustContrast(vtkDataArray* histogram, vtkDataArray* extents,
                          vtkImageData* imageData);
  void resetAutoContrastState();

  // Add placeholder nodes to make the color bar and opacity editor look nicer
  void addPlaceholderNodes();
  void removePlaceholderNodes();
  void addLUTPlaceholderNodes();
  void addOpacityPlaceholderNodes();
  void removeLUTPlaceholderNodes();
  void removeOpacityPlaceholderNodes();

  vtkNew<vtkChartHistogramColorOpacityEditor> m_histogramColorOpacityEditor;
  vtkNew<vtkContextView> m_histogramView;
  vtkNew<vtkEventQtSlotConnect> m_eventLink;
  QToolButton* m_colorLegendToolButton;
  QToolButton* m_colorMapSettingsButton;
  QToolButton* m_savePresetButton;
  QToolButton* m_autoAdjustContrastButton;

  vtkWeakPointer<vtkDiscretizableColorTransferFunction> m_LUT;
  vtkWeakPointer<vtkPiecewiseFunction> m_scalarOpacityFunction;
  vtkWeakPointer<vtkSMProxy> m_LUTProxy;
  vtkWeakPointer<vtkTable> m_inputData;

  PresetDialog* m_presetDialog = nullptr;
  QVTKGLWidget* m_qvtk;
  QScopedPointer<QDialog> m_colorMapSettingsDialog;
  ColorMapSettingsWidget* m_colorMapSettingsWidget = nullptr;

  // To prevent infinite recursion...
  bool m_updatingColorFunction = false;

  bool m_firstColorNodeIsPlaceholder = false;
  bool m_lastColorNodeIsPlaceholder = false;
  bool m_firstOpacityNodeIsPlaceholder = false;
  bool m_lastOpacityNodeIsPlaceholder = false;

  static const int m_defaultAutoContrastThreshold = 5000;
  int m_currentAutoContrastThreshold = 5000;
};
} // namespace tomviz

#endif // tomvizHistogramWidget_h
