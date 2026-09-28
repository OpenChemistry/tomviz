/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOpacityPresetWidget_h
#define tomvizOpacityPresetWidget_h

#include <QScopedPointer>
#include <QWidget>

#include <memory>

class vtkDiscretizableColorTransferFunction;

namespace tomviz {

namespace pipeline {
class VolumeData;
using VolumeDataPtr = std::shared_ptr<VolumeData>;
} // namespace pipeline

/// Replace the scalar opacity function with a prescribed shape
/// (Gaussian, linear ramp, or linear ramp above a cutoff) over the data
/// range, with sliders for its center and width. Edits apply live; the
/// histogram editor picks them up through the function's Modified event.
class OpacityPresetWidget : public QWidget
{
  Q_OBJECT

public:
  OpacityPresetWidget(pipeline::VolumeDataPtr volumeData,
                      vtkDiscretizableColorTransferFunction* lut,
                      QWidget* parent = nullptr);
  ~OpacityPresetWidget() override;

  void setVolumeData(pipeline::VolumeDataPtr volumeData);
  void setLut(vtkDiscretizableColorTransferFunction* lut);
  void updateGui();

private:
  class Internals;
  QScopedPointer<Internals> m_internals;
};

} // namespace tomviz

#endif
