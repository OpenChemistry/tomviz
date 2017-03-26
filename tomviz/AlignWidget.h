/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizAlignWidget_h
#define tomvizAlignWidget_h

#include "EditOperatorWidget.h"

#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkVector.h>

#include <QPointer>
#include <QVector>

class QLabel;
class QComboBox;
class QSpinBox;
class QTimer;
class QKeyEvent;
class QButtonGroup;
class QPushButton;
class QRadioButton;
class QTableWidget;

class vtkImageData;
class vtkImageSlice;
class vtkImageSliceMapper;
class vtkInteractorStyleRubberBand2D;
class vtkInteractorStyleRubberBandZoom;
class vtkRenderer;
class QVTKOpenGLWidget;

namespace tomviz {

class DataSource;
class SpinBox;
class TranslateAlignOperator;
class ViewMode;

class AlignWidget : public EditOperatorWidget
{
  Q_OBJECT

public:
  AlignWidget(TranslateAlignOperator* op, vtkSmartPointer<vtkImageData> data,
              QWidget* parent = nullptr);
  ~AlignWidget() override;

  // This will filter the QVTKOpenGLWidget events
  bool eventFilter(QObject* object, QEvent* event) override;

  void applyChangesToOperator() override;

protected:
  void changeSlice(int delta);
  void setSlice(int slice, bool resetInc = true);
  void widgetKeyPress(QKeyEvent* key);
  void applySliceOffset(int sliceNumber = -1);

  void zoomToSelectionFinished();

protected slots:
  void changeMode(int mode);
  void updateReference();
  void setFrameRate(int rate);
  void currentSliceEdited();

  void startAlign();
  void stopAlign();
  void onTimeout();

  void zoomToSelectionStart();

  void resetCamera();

  void onPresetClicked();
  void applyCurrentPreset();

  void sliceOffsetEdited(int slice, int offsetComponent);

protected:
  vtkNew<vtkImageSlice> m_imageSlice;
  vtkNew<vtkImageSliceMapper> m_mapper;
  vtkNew<vtkRenderer> m_renderer;
  vtkNew<vtkInteractorStyleRubberBand2D> m_defaultInteractorStyle;
  vtkNew<vtkInteractorStyleRubberBandZoom> m_zoomToBoxInteractorStyle;
  vtkSmartPointer<vtkImageData> m_inputData;
  QVTKOpenGLWidget* m_widget;

  QComboBox* m_modeSelect;
  QTimer* m_timer;
  SpinBox* m_currentSlice;
  QLabel* m_currentSliceOffset;
  QButtonGroup* m_referenceSliceMode;
  QRadioButton* m_prevButton;
  QRadioButton* m_nextButton;
  QRadioButton* m_statButton;
  QSpinBox* m_refNum;
  QPushButton* m_startButton;
  QPushButton* m_stopButton;
  QTableWidget* m_offsetTable;

  int m_frameRate;
  int m_referenceSlice;
  int m_observerId;

  int m_maxSliceNum;
  int m_minSliceNum;

  QVector<ViewMode*> m_modes;
  int m_currentMode;

  QVector<vtkVector2i> m_offsets;
  QPointer<TranslateAlignOperator> m_operator;
  DataSource* m_unalignedData;
  DataSource* m_alignedData;
};
}

#endif // tomvizAlignWidget
