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
#ifndef tomvizvtkChartHistogram_h
#define tomvizvtkChartHistogram_h

#include "vtkChartXY.h"

#include "vtkNew.h"
#include "vtkTransform2D.h"

class vtkContextMouseEvent;
class vtkHistogramMarker;

//-----------------------------------------------------------------------------
class vtkChartHistogram : public vtkChartXY
{
public:
  static vtkChartHistogram * New();

  bool MouseDoubleClickEvent(const vtkContextMouseEvent &mouse) override;

  vtkNew<vtkTransform2D> Transform;
  double PositionX;
  vtkNew<vtkHistogramMarker> Marker;

private:
  vtkChartHistogram();
};

#endif // tomvizvtkCharHistogram_h
