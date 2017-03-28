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
#ifndef tomvizvtkChartGradientOpacityEditor_h
#define tomvizvtkChartGradientOpacityEditor_h

#include <vtkAbstractContextItem.h>
#include <vtkNew.h>

class vtkAxis;
class vtkChartHistogram;
class vtkChartXY;
class vtkPiecewiseFunction;
class vtkScalarsToColors;
class vtkTable;

// This class is a chart that combines a histogram from a data set
// a color bar editor, and an opacity editor.
class vtkChartGradientOpacityEditor : public vtkAbstractContextItem
{
public:
  vtkTypeMacro(
    vtkChartGradientOpacityEditor,
    vtkAbstractContextItem) static vtkChartGradientOpacityEditor* New();

  // Set the input data.
  void SetHistogramInputData(vtkTable* table, const char* xAxisColumn,
                             const char* yAxisColumn);

  // Enable or disable scalar visibility.
  virtual void SetScalarVisibility(bool visible);

  // Set the name of the array by which the histogram should be colored.
  virtual void SelectColorArray(const char* arrayName);

  // Set the opacity function.
  virtual void SetOpacityFunction(vtkPiecewiseFunction* opacityFunction);

  // Get an axis from the histogram chart.
  vtkAxis* GetHistogramAxis(int axis);

  // Set the DPI of the chart.
  void SetDPI(int dpi);

  // Paint event for the editor.
  virtual bool Paint(vtkContext2D* painter) override;

protected:
  // This provides the histogram, contour value marker, and opacity editor.
  vtkNew<vtkChartHistogram> HistogramChart;

private:
  vtkChartGradientOpacityEditor();
  ~vtkChartGradientOpacityEditor() override;

  class PIMPL;
  PIMPL* Private;

  float Borders[4];
};

#endif // tomvizvtkChartGradientOpacityEditor_h
