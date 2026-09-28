/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizBrightnessContrastWidget_h
#define tomvizBrightnessContrastWidget_h

#include <QScopedPointer>
#include <QWidget>

#include <memory>

class vtkDiscretizableColorTransferFunction;

namespace tomviz {

namespace pipeline {
class VolumeData;
using VolumeDataPtr = std::shared_ptr<VolumeData>;
} // namespace pipeline

class BrightnessContrastWidget : public QWidget
{
  // Widget to edit the brightness and contrast of a color map.
  // This is performed by moving the min and max around.
  // Connect to the vtkDiscretizableColorTransferFunction Modified event to
  // be notified when updates occur.

  Q_OBJECT
  typedef QWidget Superclass;

public:
  BrightnessContrastWidget(pipeline::VolumeDataPtr volumeData,
                           vtkDiscretizableColorTransferFunction* lut,
                           QWidget* parent = nullptr);
  virtual ~BrightnessContrastWidget();

  void setVolumeData(pipeline::VolumeDataPtr volumeData);
  void setLut(vtkDiscretizableColorTransferFunction* lut);

  void updateGui();

signals:
  void autoPressed();
  void resetPressed();
  /// Auto-contrast using only the voxels inside a region the user
  /// picks, rather than the whole volume.
  void autoRegionPressed();

private:
  class Internals;
  QScopedPointer<Internals> m_internals;
};
} // namespace tomviz

#endif
