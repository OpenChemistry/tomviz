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
#include "ScaleLegend.h"

#include <vtkAxisActor2D.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkDistanceWidget.h>
#include <vtkHandleWidget.h>
#include <vtkLengthScaleRepresentation.h>
#include <vtkMath.h>
#include <vtkPointHandleRepresentation2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkVolumeScaleRepresentation.h>

#include <pqView.h>
#include <vtkPVAxesWidget.h>
#include <vtkPVRenderView.h>
#include <vtkSMViewProxy.h>

#include "ActiveObjects.h"
#include "DataSource.h"
#include "MainWindow.h"
#include "ModuleManager.h"
#include "Utilities.h"

#include <cmath>

// The Scale legend lives in a sub-render window at the bottom right-hand corner
// of the viewing screen and has its own camera. vtkLinkCameras connects the
// sub-render window camera to the main window camera.
class vtkLinkCameras : public vtkCommand
{
public:
  static vtkLinkCameras* New()
  {
    vtkLinkCameras* cb = new vtkLinkCameras;
    return cb;
  }

  vtkLinkCameras() : Style(tomviz::ScaleLegendStyle::Cube) {}

  void SetParentCamera(vtkCamera* c) { this->ParentCamera = c; }

  void SetChildCamera(vtkCamera* c) { this->ChildCamera = c; }

  void SetScaleLegendStyle(tomviz::ScaleLegendStyle style)
  {
    this->Style = style;
  }

  virtual void Execute(vtkObject* vtkNotUsed(caller),
                       unsigned long vtkNotUsed(eventId),
                       void* vtkNotUsed(callData))
  {
    this->OrientCamera();
  }

  void OrientCamera()
  {
    double pos[3], fp[3], viewup[3];
    this->ParentCamera->GetPosition(pos);
    this->ParentCamera->GetFocalPoint(fp);
    this->ParentCamera->GetViewUp(viewup);

    if (this->Style == tomviz::ScaleLegendStyle::Cube) {
      // The child camera's focal point is always the origin, and its position
      // is translated to have the same positional offset (both distance and
      // orientation) between the camera and the object.
      for (int i = 0; i < 3; i++) {
        pos[i] -= fp[i];
        fp[i] = 0.;
      }
      this->ChildCamera->SetPosition(pos);
      this->ChildCamera->SetFocalPoint(fp);
      this->ChildCamera->SetViewUp(viewup);
    } else if (this->Style == tomviz::ScaleLegendStyle::Ruler) {
      // The child camera's focal point is always the origin, and its position
      // is translated to have the same distance (but not orientation) between
      // the camera and the object. The camera's view is fixed.
      double toFP[3];
      this->ChildCamera->GetDirectionOfProjection(toFP);
      double dist = sqrt(vtkMath::Distance2BetweenPoints(pos, fp));
      for (int i = 0; i < 3; i++) {
        pos[i] = 0.;
        fp[i] = 0.;
      }
      pos[2] = -dist;
      this->ChildCamera->SetPosition(pos);
      this->ChildCamera->SetFocalPoint(fp);
      double up[3] = { 0., 1., 0. };
      this->ChildCamera->SetViewUp(up);
    }
  }

private:
  vtkCamera* ParentCamera;
  vtkCamera* ChildCamera;

  tomviz::ScaleLegendStyle Style;
};

namespace tomviz {

ScaleLegend::ScaleLegend(QMainWindow* mw) : QObject(mw), m_mainWindow(mw)
{
  // Connect the data manager's "dataSourceAdded" to our "dataSourceAdded" slot
  // to allow us to connect to the new data source's length scale information.
  ModuleManager& mm = ModuleManager::instance();
  connect(&mm, SIGNAL(dataSourceAdded(DataSource*)),
          SLOT(dataSourceAdded(DataSource*)));

  // Measurement cube
  {
    m_volumeScaleRep->GetLabel()->GetTextProperty()->SetFontSize(30);
    m_volumeScaleRep->SetMinRelativeCubeScreenArea(.0002);
    m_volumeScaleRep->SetMaxRelativeCubeScreenArea(.002);

    m_handleWidget->CreateDefaultRepresentation();
    m_handleWidget->SetRepresentation(m_volumeScaleRep.Get());
    m_handleWidget->SetProcessEvents(0);
  }

  // Ruler
  {
    m_lengthScaleRep->InstantiateHandleRepresentation();
    double p1[3] = { -.5, 0., 0. };
    double p2[3] = { .5, 0., 0. };
    m_lengthScaleRep->SetPoint1WorldPosition(p1);
    m_lengthScaleRep->SetPoint2WorldPosition(p2);

    m_lengthScaleRep->GetAxis()->SetTickLength(9);
    m_lengthScaleRep->GetLabel()->GetTextProperty()->SetFontSize(30);
    m_lengthScaleRep->SetMinRelativeScreenWidth(.03);
    m_lengthScaleRep->SetMaxRelativeScreenWidth(.07);

    m_distanceWidget->SetRepresentation(m_lengthScaleRep.Get());
  }

  m_renderer->SetViewport(0.85, 0.0, 1.0, 0.225);

  // From vtkPVAxesWidget.cxx:
  // Since Layer==1, the renderer is treated as transparent and
  // vtkOpenGLRenderer::Clear() won't clear the color-buffer.
  m_renderer->SetLayer(vtkPVAxesWidget::RendererLayer);
  // Leaving Erase==1 ensures that the depth buffer is cleared. This ensures
  // that the orientation widget always stays on top of the rendered scene.
  m_renderer->EraseOn();
  m_renderer->InteractiveOff();
  m_renderer->AddActor(m_volumeScaleRep.Get());
  m_renderer->AddActor(m_lengthScaleRep.Get());

  // Add our sub-renderer to the main renderer
  auto view = ActiveObjects::instance().activeView();
  if (!view) {
    // Something is wrong with the view, exit early.
    return;
  }
  auto renderView = vtkPVRenderView::SafeDownCast(view->GetClientSideView());
  renderView->GetRenderWindow()->AddRenderer(m_renderer.Get());

  // Set up interactors
  m_handleWidget->SetInteractor(renderView->GetInteractor());
  m_distanceWidget->SetInteractor(renderView->GetInteractor());

  // Set up link between the cameras of the two views
  m_linkCameras->SetChildCamera(m_renderer->GetActiveCamera());
  m_linkCameras->SetParentCamera(renderView->GetActiveCamera());
  m_linkCameras->SetScaleLegendStyle(m_style);
  m_linkCamerasId = renderView->GetActiveCamera()->AddObserver(
    vtkCommand::ModifiedEvent, m_linkCameras.Get());

  // Set initial values
  setStyle(ScaleLegendStyle::Cube);
  setVisibility(m_visible);
}

ScaleLegend::~ScaleLegend()
{
  // Break the connection between the cameras of the two views
  auto view = ActiveObjects::instance().activeView();
  auto renderView = vtkPVRenderView::SafeDownCast(view->GetClientSideView());
  if (renderView) {
    renderView->GetActiveCamera()->RemoveObserver(m_linkCamerasId);
  }
}

void ScaleLegend::setStyle(ScaleLegendStyle style)
{
  m_style = style;
  m_linkCameras->SetScaleLegendStyle(m_style);

  if (m_style == ScaleLegendStyle::Cube) {
    m_volumeScaleRep->SetRepresentationVisibility(m_visible ? 1 : 0);
    m_lengthScaleRep->SetRepresentationVisibility(0);
  } else if (m_style == ScaleLegendStyle::Ruler) {
    m_lengthScaleRep->SetRepresentationVisibility(m_visible ? 1 : 0);
    m_volumeScaleRep->SetRepresentationVisibility(0);
  }
  m_linkCameras->OrientCamera();
  render();
}

void ScaleLegend::setVisibility(bool choice)
{
  m_visible = choice;

  if (m_style == ScaleLegendStyle::Cube) {
    m_volumeScaleRep->SetRepresentationVisibility(choice);
  } else if (m_style == ScaleLegendStyle::Ruler) {
    m_lengthScaleRep->SetRepresentationVisibility(choice);
  }

  render();
}

void ScaleLegend::dataSourceAdded(DataSource* ds)
{
  m_volumeScaleRep->SetLengthUnit(ds->getUnits().toStdString().c_str());
  m_lengthScaleRep->SetLengthUnit(ds->getUnits().toStdString().c_str());
  connect(ds, SIGNAL(dataPropertiesChanged()), SLOT(dataPropertiesChanged()));
  render();
}

void ScaleLegend::dataPropertiesChanged()
{
  auto data = qobject_cast<DataSource*>(sender());
  if (!data) {
    return;
  }
  m_volumeScaleRep->SetLengthUnit(data->getUnits().toStdString().c_str());
  m_lengthScaleRep->SetLengthUnit(data->getUnits().toStdString().c_str());
}

void ScaleLegend::render()
{
  auto view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}
} // namespace tomviz
