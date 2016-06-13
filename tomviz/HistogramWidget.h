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
#ifndef tomvizHistogramWidget_h
#define tomvizHistogramWidget_h

#include <QWidget>

#include <vtkNew.h>

class vtkChartHistogramColorOpacityEditor;
class vtkContextView;
class vtkEventQtSlotConnect;
class vtkPiecewiseFunction;
class vtkObject;
class vtkTable;

class QVTKWidget;

class vtkPVDiscretizableColorTransferFunction;

namespace tomviz
{

class HistogramWidget : public QWidget
{
  Q_OBJECT

public:
  explicit HistogramWidget(QWidget *parent = 0);
  ~HistogramWidget() override;

  void setLUT(vtkPVDiscretizableColorTransferFunction *lut);

  void setInputData(vtkTable *table, const char* x, const char* y);

signals:

public slots:
  void onScalarOpacityFunctionChanged();
  void onCurrentPointEditEvent();
  void histogramClicked(vtkObject *);

private:
  vtkNew<vtkChartHistogramColorOpacityEditor> HistogramColorOpacityEditor;
  vtkNew<vtkContextView> HistogramView;
  vtkNew<vtkEventQtSlotConnect> EventLink;

  vtkPVDiscretizableColorTransferFunction *LUT = nullptr;
  vtkPiecewiseFunction *ScalarOpacityFunction = nullptr;

  QVTKWidget *qvtk;
};

}

#endif // tomvizHistogramWidget_h
