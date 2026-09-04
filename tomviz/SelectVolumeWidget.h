/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSelectVolumeWidget_h
#define tomvizSelectVolumeWidget_h

#include <QScopedPointer>
#include <QWidget>

class vtkObject;

namespace tomviz {

class SelectVolumeWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  SelectVolumeWidget(const double origin[3], const double spacing[3],
                     const int extent[6], const int currentVolume[6],
                     const double position[3], QWidget* parent = nullptr);
  virtual ~SelectVolumeWidget();

  // Gets the bounds of the selection in real space (taking into account
  // the origin and spacing of the image)
  void getBoundsOfSelection(double bounds[6]);
  // Gets the bounds of the selection as extents of interest.  This is
  // the region of interest in terms of the extent given in the input
  // without the origin and spacing factored in.
  void getExtentOfSelection(int extent[6]);

  /// Show or hide the box in the 3D view. It follows this widget's own
  /// visibility by default, so a hidden or closed dialog leaves no box.
  void setBoxEnabled(bool enabled);

public slots:
  void dataMoved(double newX, double newY, double newZ);

protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

private slots:
  void interactionEnd(vtkObject* caller);
  void valueChanged();
  void updateBounds(int* bounds);
  void updateBounds(double* bounds);

private:
  class CWInternals;
  QScopedPointer<CWInternals> Internals;
};
} // namespace tomviz

#endif
