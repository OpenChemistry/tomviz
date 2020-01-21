/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSliceViewDialog_h
#define tomvizSliceViewDialog_h

#include <QDialog>

#include <QPointer>

#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

class QRadioButton;

class vtkImageData;
class vtkImageSlice;
class vtkImageSliceMapper;
class vtkRenderer;
class vtkScalarsToColors;

namespace tomviz {

class QVTKGLWidget;

class SliceViewDialog : public QDialog
{
  Q_OBJECT
public:
  SliceViewDialog(QWidget* parent = nullptr);
  ~SliceViewDialog();

  void setActiveImageData(vtkImageData* image);
  void setSliceNumber(int slice);
  void setLookupTable(vtkScalarsToColors* lut);

  void setDarkImage(vtkImageData* image) { m_darkImage = image; }
  void setWhiteImage(vtkImageData* image) { m_whiteImage = image; }

  void switchToDark();
  void switchToWhite();

private:
  void setupConnections();
  void updateLUTRange();

  vtkWeakPointer<vtkImageData> m_darkImage;
  vtkWeakPointer<vtkImageData> m_whiteImage;

  QPointer<QVTKGLWidget> m_glWidget;
  QPointer<QRadioButton> m_darkButton;
  QPointer<QRadioButton> m_whiteButton;

  vtkNew<vtkImageSlice> m_slice;
  vtkNew<vtkImageSliceMapper> m_mapper;
  vtkNew<vtkRenderer> m_renderer;

  vtkSmartPointer<vtkScalarsToColors> m_lut;
};
} // namespace tomviz

#endif
