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
#include <vtkVector.h>

#include <QVector>
#include <QPointer>

class QLabel;
class QSpinBox;
class QTimer;
class QKeyEvent;
class QButtonGroup;
class QPushButton;
class QRadioButton;

class vtkImageSlice;
class vtkImageSliceMapper;
class vtkInteractorStyleRubberBand2D;
class vtkInteractorStyleRubberBandZoom;
class vtkRenderer;
class QVTKWidget;

namespace tomviz
{

class DataSource;
class TranslateAlignOperator;

class AlignWidget : public EditOperatorWidget
{
  Q_OBJECT

public:
  AlignWidget(TranslateAlignOperator *op, QWidget* parent = nullptr);
  ~AlignWidget();

  // This will filter the QVTKWidget events
  bool eventFilter(QObject *object, QEvent *event) override;

  void applyChangesToOperator() override;

protected slots:
  void changeSlice();
  void changeSlice(int delta);
  void setSlice(int slice, bool resetInc = true);
  void updateReference();
  void setFrameRate(int rate);
  void widgetKeyPress(QKeyEvent *key);
  void applySliceOffset(int sliceNumber = -1);
  void startAlign();
  void stopAlign();

  void zoomToSelectionStart();
  void zoomToSelectionFinished();

  void resetCamera();

protected:
  vtkNew<vtkImageSlice> imageSlice;
  vtkNew<vtkImageSliceMapper> mapper;
  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkInteractorStyleRubberBand2D> defaultInteractorStyle;
  vtkNew<vtkInteractorStyleRubberBandZoom> zoomToBoxInteractorStyle;
  QVTKWidget *widget;

  QTimer *timer;
  QSpinBox *currentSlice;
  QLabel *currentSliceOffset;
  QButtonGroup *referenceSliceMode;
  QRadioButton *prevButton;
  QRadioButton *nextButton;
  QRadioButton *statButton;
  QSpinBox *statRefNum;
  QPushButton *startButton;
  QPushButton *stopButton;

  int frameRate;
  int referenceSlice;
  int observerId;

  QVector<vtkVector2i> offsets;
  QPointer<TranslateAlignOperator> Op;
  DataSource *unalignedData;
  DataSource *alignedData;
};

}

#endif // tomvizAlignWidget
