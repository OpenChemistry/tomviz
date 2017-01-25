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

class QVTKOpenGLWidget;

class vtkPVDiscretizableColorTransferFunction;
class vtkSMProxy;

namespace tomviz {

class HistogramWidget : public QWidget
{
  Q_OBJECT

public:
  explicit HistogramWidget(QWidget* parent = 0);
  ~HistogramWidget() override;

  void setLUT(vtkPVDiscretizableColorTransferFunction* lut);
  void setLUTProxy(vtkSMProxy* proxy);

  void setInputData(vtkTable* table, const char* x, const char* y);

signals:
  void colorMapUpdated();

public slots:
  void onScalarOpacityFunctionChanged();
  void onCurrentPointEditEvent();
  void histogramClicked(vtkObject*);

  void onResetRangeClicked();
  void onCustomRangeClicked();
  void onInvertClicked();
  void onPresetClicked();
  void applyCurrentPreset();

private:
  void renderViews();
  vtkNew<vtkChartHistogramColorOpacityEditor> m_histogramColorOpacityEditor;
  vtkNew<vtkContextView> m_histogramView;
  vtkNew<vtkEventQtSlotConnect> m_eventLink;

  vtkPVDiscretizableColorTransferFunction* m_LUT = nullptr;
  vtkPiecewiseFunction* m_scalarOpacityFunction = nullptr;
  vtkSMProxy* m_LUTProxy = nullptr;

  QVTKOpenGLWidget* m_qvtk;
};
}

#endif // tomvizHistogramWidget_h
