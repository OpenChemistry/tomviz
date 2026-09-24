/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QTest>
#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServer.h>
#include <pqServerResource.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkOutputWindow.h>
#include <vtkPVRenderView.h>
#include <vtkPVView.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkStringOutputWindow.h>

#include "ClippingRangeBehavior.h"

#include <array>
#include <string>

using namespace tomviz;

// Tomviz visualizations add their props to the renderer directly, not as
// ParaView representations, so ParaView's clipping range never saw them
// grow or move.
class ClippingRangeTest : public QObject
{
  Q_OBJECT

private slots:
  // Same OpenGL probe as LabelSurfaceRenderTest: CI runners without GL
  // crash on the first render, so check before any view exists.
  void initTestCase()
  {
    vtkNew<vtkStringOutputWindow> capture;
    auto* previous = vtkOutputWindow::GetInstance();
    previous->Register(nullptr);
    vtkOutputWindow::SetInstance(capture);
    {
      vtkNew<vtkRenderWindow> probe;
      probe->SetOffScreenRendering(1);
      probe->Initialize();
    }
    vtkOutputWindow::SetInstance(previous);
    previous->Delete();
    const std::string said = capture->GetOutput();
    for (const char* complaint : { "OpenGL", "X server", "EGL", "OSMesa",
                                   "pixel format", "PixelFormat" }) {
      if (said.find(complaint) != std::string::npos) {
        QSKIP("no OpenGL available for an offscreen render");
      }
    }
  }

  // A render view proxy with a real offscreen window. The offscreen Qt
  // platform cannot draw through a Qt GL widget, so this skips pqView and
  // renders through the proxy, which is the path the application's
  // renders take (vtkSMViewProxy::StillRender).
  void init()
  {
    auto* pxm =
      pqActiveObjects::instance().activeServer()->proxyManager();
    m_proxy.TakeReference(
      vtkSMViewProxy::SafeDownCast(pxm->NewProxy("views", "RenderView")));
    QVERIFY(m_proxy);
    m_proxy->UpdateVTKObjects();
    m_pvView = vtkPVRenderView::SafeDownCast(m_proxy->GetClientSideView());
    QVERIFY(m_pvView);
    auto* window = m_proxy->GetRenderWindow();
    window->SetOffScreenRendering(1);
    window->SetSize(200, 200);
    ClippingRangeBehavior::watch(m_proxy);
  }

  void cleanup()
  {
    m_pvView = nullptr;
    m_proxy = nullptr;
  }

  // A live source's data growing between renders
  void growingDataIsNotClipped()
  {
    vtkNew<vtkSphereSource> sphere;
    sphere->SetRadius(1.0);
    addActor(sphere);
    lookFrom(200.0);
    m_proxy->StillRender();
    expectCovers(199.0, 201.0);
    // Fitted, not the camera's default range: the frame really rendered
    QVERIFY(clippingRange()[0] > 150.0);

    sphere->SetRadius(50.0);
    m_proxy->StillRender();
    expectCovers(150.0, 250.0);
  }

  // A new dataset or a transform's output appearing somewhere else
  void dataAddedElsewhereIsNotClipped()
  {
    vtkNew<vtkSphereSource> near;
    near->SetRadius(1.0);
    addActor(near);
    lookFrom(10.0);
    m_proxy->StillRender();
    expectCovers(9.0, 11.0);
    QVERIFY(clippingRange()[1] < 50.0);

    vtkNew<vtkSphereSource> far;
    far->SetCenter(0.0, 0.0, -100.0);
    far->SetRadius(1.0);
    addActor(far);
    m_proxy->StillRender();
    expectCovers(9.0, 101.0);
  }

private:
  void addActor(vtkSphereSource* source)
  {
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(source->GetOutputPort());
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    m_pvView->AddPropToRenderer(actor);
  }

  void lookFrom(double z)
  {
    auto* camera = m_pvView->GetActiveCamera();
    camera->SetFocalPoint(0.0, 0.0, 0.0);
    camera->SetPosition(0.0, 0.0, z);
    camera->SetViewUp(0.0, 1.0, 0.0);
  }

  std::array<double, 2> clippingRange()
  {
    std::array<double, 2> range;
    m_pvView->GetActiveCamera()->GetClippingRange(range.data());
    return range;
  }

  // The near and far planes lie outside [nearest, farthest] distances
  void expectCovers(double nearest, double farthest)
  {
    const auto range = clippingRange();
    QVERIFY2(range[0] <= nearest && range[1] >= farthest,
             qPrintable(QString("clipping range %1 to %2 does not cover "
                                "%3 to %4")
                          .arg(range[0])
                          .arg(range[1])
                          .arg(nearest)
                          .arg(farthest)));
  }

  vtkSmartPointer<vtkSMViewProxy> m_proxy;
  vtkPVRenderView* m_pvView = nullptr;
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  // A plain render window that can draw offscreen, not the one a Qt
  // widget hosts
  vtkPVView::SetUseGenericOpenGLRenderWindow(false);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  ClippingRangeTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "ClippingRangeTest.moc"
