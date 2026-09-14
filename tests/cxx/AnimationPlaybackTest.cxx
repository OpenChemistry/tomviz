/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QTest>

#include <pqActiveObjects.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqSMAdaptor.h>
#include <pqServer.h>
#include <pqServerResource.h>

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkSMProxy.h>
#include <vtkSMRenderViewProxy.h>

#include "CameraViewpoints.h"
#include "ModuleAnimation.h"

using namespace tomviz;

namespace {

/// Records what a module animation sees on every tick of a playback.
class Probe : public ModuleAnimation
{
public:
  Probe() : ModuleAnimation(nullptr) {}

  void onTimeChanged() override
  {
    progressSamples.append(progress());
    if (view) {
      double pos[3];
      view->getRenderViewProxy()->GetActiveCamera()->GetPosition(pos);
      cameraX.append(pos[0]);
    }
  }

  QList<double> progressSamples;
  QList<double> cameraX;
  pqRenderView* view = nullptr;
};

Viewpoint viewpointAt(double x)
{
  Viewpoint viewpoint;
  viewpoint.position = { x, 0, 10 };
  viewpoint.focalPoint = { x, 0, 0 };
  viewpoint.eased = false;
  return viewpoint;
}

pqAnimationScene* scene()
{
  return pqPVApplicationCore::instance()->animationManager()->getActiveScene();
}

void play(int frames)
{
  auto* proxy = scene()->getProxy();
  pqSMAdaptor::setEnumerationProperty(proxy->GetProperty("PlayMode"),
                                      "Sequence");
  pqSMAdaptor::setElementProperty(proxy->GetProperty("NumberOfFrames"),
                                  frames);
  proxy->UpdateVTKObjects();
  proxy->InvokeCommand("Play");
}

} // namespace

class AnimationPlaybackTest : public QObject
{
  Q_OBJECT

private slots:
  void cleanup() { CameraViewpoints::instance().clear(); }

  // The whole animation system hangs off pqTimeKeeper::timeChanged and
  // ModuleAnimation::progress(). A plain Sequence playback with no time
  // series loaded must tick every frame and run progress from 0 to 1.
  void playbackTicksModuleAnimationsFromStartToEnd()
  {
    Probe probe;
    play(10);

    // The time keeper only announces changes, so a playback that starts
    // where the clock already sits (time 0 on a fresh session) does not
    // tick its first frame; startFlight's snapToHead covers that. From
    // then on every frame ticks, through to the end.
    QVERIFY2(probe.progressSamples.size() >= 5,
             qPrintable(QString("only %1 ticks").arg(probe.progressSamples.size())));
    QVERIFY(probe.progressSamples.first() < 0.2);
    QVERIFY(probe.progressSamples.last() > 0.95);
    for (int i = 1; i < probe.progressSamples.size(); ++i) {
      QVERIFY(probe.progressSamples[i] >= probe.progressSamples[i - 1]);
    }
  }

  // Two viewpoints and an armed flight: playing must carry the camera
  // from the first to the second, through positions in between.
  void cameraFliesBetweenViewpointsDuringPlayback()
  {
    auto* server = pqActiveObjects::instance().activeServer();
    QVERIFY(server);
    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    auto* view = qobject_cast<pqRenderView*>(
      builder->createView(pqRenderView::renderViewType(), server));
    QVERIFY(view);

    auto& viewpoints = CameraViewpoints::instance();
    viewpoints.append(viewpointAt(0.0));
    viewpoints.append(viewpointAt(10.0));
    // As addViewpoint does: armed, no snap to the head
    viewpoints.startFlight(view, /*snapToHead=*/false);
    QVERIFY(viewpoints.isFlying());

    // Connected after the flight, so each tick reads the camera the
    // flight just moved
    // The interpolation itself, independent of any view: a straight
    // line between the two, sampled where the camera should be
    vtkNew<vtkCamera> camera;
    for (double t : { 0.0, 0.25, 0.5, 0.75, 1.0 }) {
      viewpoints.interpolate(t, camera);
      double pos[3];
      camera->GetPosition(pos);
      QVERIFY2(std::abs(pos[0] - 10.0 * t) < 1e-6,
               qPrintable(QString("t=%1 x=%2").arg(t).arg(pos[0])));
      QVERIFY(std::abs(pos[2] - 10.0) < 1e-6);
    }

    // Three viewpoints curve through the middle one
    viewpoints.append(viewpointAt(20.0));
    viewpoints.interpolate(0.5, camera);
    double mid[3];
    camera->GetPosition(mid);
    QVERIFY(std::abs(mid[0] - 10.0) < 1e-6);
    viewpoints.removeAt(2);

    Probe probe;
    probe.view = view;
    play(10);

    QVERIFY(probe.cameraX.size() >= 5);
    QVERIFY(std::abs(probe.cameraX.last() - 10.0) < 0.5);
    bool between = false;
    for (double x : probe.cameraX) {
      between = between || (x > 2.0 && x < 8.0);
    }
    QVERIFY2(between, "the camera jumped rather than flew");

    viewpoints.stopFlight();
    builder->destroy(view);
  }
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  AnimationPlaybackTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "AnimationPlaybackTest.moc"
