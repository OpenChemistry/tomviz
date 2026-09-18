/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizHistogramWidget_h
#define tomvizHistogramWidget_h

#include <QPointer>
#include <QWidget>

#include <vtkNew.h>
#include <vtkWeakPointer.h>

#include <memory>

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

namespace pipeline {
class VolumeData;
using VolumeDataPtr = std::shared_ptr<VolumeData>;
} // namespace pipeline

class BrightnessContrastWidget;
class OpacityPresetWidget;
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
  void setVolumeData(pipeline::VolumeDataPtr volumeData);

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
  void onBrightnessAndContrastClicked();
  void onOpacityPresetsClicked();
  void onCreateSegmentationColormapClicked();
  void applyCurrentPreset();
  void updateUI();

  void updateColorMapDialogs();

protected:
  void showEvent(QShowEvent* event) override;

private:
  void renderViews();
  /// The deferred body of onScalarOpacityFunctionChanged: render, and
  /// copy the function's points into its proxy.
  void syncScalarOpacityFunction();
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
  /// Open a box selector in the render view and auto-adjust the
  /// contrast from the voxels inside it.
  void autoAdjustContrastForSelectedRegion();
  /// Auto-adjust the contrast using only @a extent (in voxel indices).
  void autoAdjustContrastForExtent(const int extent[6]);
  void resetAutoContrastState();

  // Add placeholder nodes to make the color bar and opacity editor look nicer
  void addPlaceholderNodes();
  void removePlaceholderNodes();

  vtkNew<vtkChartHistogramColorOpacityEditor> m_histogramColorOpacityEditor;
  vtkNew<vtkContextView> m_histogramView;
  vtkNew<vtkEventQtSlotConnect> m_eventLink;
  QToolButton* m_colorLegendToolButton;
  QToolButton* m_colorMapSettingsButton;
  QToolButton* m_savePresetButton;
  QToolButton* m_brightnessAndContrastButton;
  QToolButton* m_opacityPresetButton;

  vtkWeakPointer<vtkDiscretizableColorTransferFunction> m_LUT;
  vtkWeakPointer<vtkPiecewiseFunction> m_scalarOpacityFunction;
  vtkWeakPointer<vtkSMProxy> m_LUTProxy;
  vtkWeakPointer<vtkTable> m_inputData;

  PresetDialog* m_presetDialog = nullptr;
  QVTKGLWidget* m_qvtk;

  QPointer<QDialog> m_colorMapSettingsDialog;
  QPointer<ColorMapSettingsWidget> m_colorMapSettingsWidget;

  QPointer<QDialog> m_brightnessContrastDialog;
  QPointer<BrightnessContrastWidget> m_brightnessContrastWidget;
  QPointer<QDialog> m_opacityPresetDialog;
  QPointer<OpacityPresetWidget> m_opacityPresetWidget;
  QPointer<QDialog> m_autoContrastRegionDialog;

  pipeline::VolumeDataPtr m_volumeData;

  // To prevent infinite recursion...
  bool m_updatingColorFunction = false;

  // Coalesces the deferred color-function rebuild (see
  // onColorFunctionChanged) so reentrant ModifiedEvents don't pile up.
  bool m_colorFunctionUpdatePending = false;
  bool m_opacityFunctionUpdatePending = false;

  // Matches ImageJ's ContrastAdjuster: AUTO_THRESHOLD is the starting
  // threshold divisor, and the current value must begin below 10 so the
  // first "Auto" press uses the full AUTO_THRESHOLD before halving.
  static const int m_defaultAutoContrastThreshold = 5000;
  int m_currentAutoContrastThreshold = 0;
};
} // namespace tomviz

#endif // tomvizHistogramWidget_h
