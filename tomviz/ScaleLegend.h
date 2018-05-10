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
#ifndef tomvizScaleLegend_h
#define tomvizScaleLegend_h

#include <QObject>

#include <vtkNew.h>

class pqView;

class vtkLengthScaleRepresentation;
class vtkDistanceWidget;
class vtkHandleWidget;
class vtkLinkCameras;
class vtkRenderer;
class vtkSMViewProxy;
class vtkVolumeScaleRepresentation;

namespace tomviz {
class DataSource;

enum class ScaleLegendStyle : unsigned int
{
  Cube,
  Ruler
};

class ScaleLegend : public QObject
{
  Q_OBJECT

public:
  static ScaleLegend* getScaleLegend(vtkSMViewProxy* view);
  static ScaleLegend* getScaleLegend(pqView* view);

  ScaleLegendStyle style() const { return m_style; }
  bool visible() const { return m_visible; }

public slots:
  void setStyle(ScaleLegendStyle style);
  void setVisibility(bool choice);

private slots:
  void dataSourceAdded(DataSource* ds);
  void dataPropertiesChanged();

private:
  ScaleLegend(pqView* v);
  ~ScaleLegend() override;

  Q_DISABLE_COPY(ScaleLegend)

  void render();

  vtkNew<vtkDistanceWidget> m_distanceWidget;
  vtkNew<vtkLengthScaleRepresentation> m_lengthScaleRep;
  vtkNew<vtkHandleWidget> m_handleWidget;
  vtkNew<vtkLinkCameras> m_linkCameras;
  vtkNew<vtkVolumeScaleRepresentation> m_volumeScaleRep;
  vtkNew<vtkRenderer> m_renderer;
  ScaleLegendStyle m_style = ScaleLegendStyle::Cube;

  unsigned long m_linkCamerasId;
  bool m_visible = false;
};
} // namespace tomviz
#endif
