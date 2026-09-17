/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
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

#include "AnimatableProperties.h"
#include "CutOutAnimation.h"
#include "ExplodedAnimation.h"
#include "ModuleAnimations.h"
#include "ThresholdAnimation.h"
#include "OpacityAnimation.h"
#include "RecordedAnimations.h"
#include "SceneSnapshot.h"
#include "pipeline/Pipeline.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/ThresholdSink.h"
#include "pipeline/sinks/VolumeSink.h"

// The animations built from recorded viewpoint state: they need a
// ParaView core to exist, so they are exercised here rather than in the
// plain gtest suite.
class RecordedAnimationTest : public QObject
{
  Q_OBJECT

private slots:
  void cleanup()
  {
    ModuleAnimations::instance().clear();
    CameraViewpoints::instance().clear();
  }

  void recordedOpacityFadesInAndYieldsToAuthoredAnimations()
  {
    pipeline::Pipeline pipeline;
    Viewpoint first = viewpointAt(0.0);
    first.scene = SceneSnapshot::capture(&pipeline);
    CameraViewpoints::instance().append(first);

    auto* slice = new pipeline::SliceSink();
    pipeline.addNode(slice);
    slice->setOpacity(0.6);
    Viewpoint second = viewpointAt(10.0);
    second.scene = SceneSnapshot::capture(&pipeline);
    CameraViewpoints::instance().append(second);

    auto& recorded = RecordedAnimations::instance();
    recorded.sync(&pipeline);
    auto animations = ModuleAnimations::instance().animations();
    QCOMPARE(animations.size(), 1);
    auto* fade = qobject_cast<RecordedAnimation*>(animations.first());
    QVERIFY(fade);
    QVERIFY(fade->recorded());
    QCOMPARE(fade->type(), QString("opacity"));

    // Hidden at the head (keeping the opacity it will fade up to), half
    // way at the middle, arrived at the end
    fade->applyPathTime(0.0);
    QVERIFY(!slice->visibility());
    QCOMPARE(slice->opacity(), 0.6);
    fade->applyPathTime(0.5);
    QVERIFY(slice->visibility());
    QCOMPARE(slice->opacity(), 0.3);
    fade->applyPathTime(1.0);
    QVERIFY(slice->visibility());
    QCOMPARE(slice->opacity(), 0.6);

    // An authored opacity animation on the same leg takes it over: the
    // recorded one leaves the slice alone there and the list shows the
    // authored row instead
    auto* authored = new OpacityAnimation(slice, 1.0, 0.2);
    authored->segment = 0;
    ModuleAnimations::instance().add(authored);
    recorded.sync(&pipeline);
    RecordedAnimation* rebuilt = nullptr;
    for (auto* animation : ModuleAnimations::instance().animations()) {
      if (animation->recorded()) {
        rebuilt = qobject_cast<RecordedAnimation*>(animation);
      }
    }
    QVERIFY(rebuilt);
    slice->setOpacity(0.9);
    rebuilt->applyPathTime(0.5);
    QCOMPARE(slice->opacity(), 0.9);
    QVERIFY(recorded.changes(&pipeline).isEmpty());

    // A slice moved between viewpoints slides its index along the leg
    // (recorded alongside the fade, since both changed)
    slice->setSlice(10);
    Viewpoint moved = viewpointAt(20.0);
    moved.scene = SceneSnapshot::capture(&pipeline);
    slice->setSlice(50);
    CameraViewpoints::instance().replace(1, moved);
    Viewpoint third = viewpointAt(30.0);
    third.scene = SceneSnapshot::capture(&pipeline);
    CameraViewpoints::instance().append(third);
    recorded.sync(&pipeline);
    RecordedAnimation* plane = nullptr;
    for (auto* animation : ModuleAnimations::instance().animations()) {
      if (animation->recorded() && animation->type() == "slice") {
        plane = qobject_cast<RecordedAnimation*>(animation);
      }
    }
    QVERIFY(plane);
    // Stops: 0, 0.5, 1 with equal legs; the slice changes on the second
    plane->applyPathTime(0.5);
    QCOMPARE(slice->slice(), 10);
    plane->applyPathTime(0.75);
    QCOMPARE(slice->slice(), 30);
    plane->applyPathTime(1.0);
    QCOMPARE(slice->slice(), 50);
    CameraViewpoints::instance().removeAt(2);
    slice->setSlice(10);
    recorded.sync(&pipeline);

    // Not saved: rebuilt from the viewpoints instead
    auto json = ModuleAnimations::instance().serialize(&pipeline);
    QCOMPARE(json["modules"].toArray().size(), 1);

    // Removing the recorded change edits the viewpoint, and a sync then
    // has nothing recorded left to build
    ModuleAnimations::instance().remove(authored);
    recorded.sync(&pipeline);
    auto changes = recorded.changes(&pipeline);
    QCOMPARE(changes.size(), 1);
    recorded.remove(changes.first(), &pipeline);
    recorded.sync(&pipeline);
    for (auto* animation : ModuleAnimations::instance().animations()) {
      QVERIFY(!animation->recorded());
    }
  }

  // An authored exploded-view animation sweeps the gap and the chunk
  // count during playback, switching the exploded view on, and comes
  // back from a state file with its unit.
  void explodedAnimationSweepsTheViewAndSurvivesAStateFile()
  {
    pipeline::Pipeline pipeline;
    auto* volume = new pipeline::VolumeSink();
    pipeline.addNode(volume);
    volume->setExplodedGap(0.0);
    QVERIFY(!volume->explodedEnabled());

    auto& animations = ModuleAnimations::instance();
    animations.add(
      new ExplodedAnimation(volume, 0.0, 0.5, ExplodedAnimation::Gap));
    animations.add(
      new ExplodedAnimation(volume, 2, 8, ExplodedAnimation::Chunks));
    // The offset is stored as swept and clamped when the slabs are cut,
    // so it reads back as the sweep's end even without data
    animations.add(
      new ExplodedAnimation(volume, -3, 3, ExplodedAnimation::Offset));

    play(10);
    QVERIFY(volume->explodedEnabled());
    // The last tick lands at progress 1 or just short of it
    QVERIFY2(std::abs(volume->explodedGap() - 0.5) < 0.06,
             qPrintable(QString("gap %1").arg(volume->explodedGap())));
    QCOMPARE(volume->explodedChunks(), 8);

    QCOMPARE(volume->explodedOffset(), 3);

    auto json = animations.serialize(&pipeline);
    auto entries = json["modules"].toArray();
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].toObject()["type"].toString(), QString("exploded"));
    QCOMPARE(entries[0].toObject()["unit"].toString(), QString("gap"));
    QCOMPARE(entries[1].toObject()["unit"].toString(), QString("chunks"));
    QCOMPARE(entries[2].toObject()["unit"].toString(), QString("offset"));

    animations.deserialize(json, &pipeline);
    auto restored = animations.animations();
    QCOMPARE(restored.size(), 3);
    auto* gap = qobject_cast<ExplodedAnimation*>(restored[0]);
    QVERIFY(gap);
    QCOMPARE(gap->unit, ExplodedAnimation::Gap);
    QCOMPARE(gap->startValue, 0.0);
    QCOMPARE(gap->stopValue, 0.5);
    QCOMPARE(gap->baseNode.data(), volume);
    auto* chunks = qobject_cast<ExplodedAnimation*>(restored[1]);
    QVERIFY(chunks);
    QCOMPARE(chunks->unit, ExplodedAnimation::Chunks);
    QCOMPARE(chunks->stopValue, 8.0);
    auto* offset = qobject_cast<ExplodedAnimation*>(restored[2]);
    QVERIFY(offset);
    QCOMPARE(offset->unit, ExplodedAnimation::Offset);
    QCOMPARE(offset->startValue, -3.0);

    // Only a volume can be exploded
    auto* slice = new pipeline::SliceSink();
    pipeline.addNode(slice);
    QJsonObject wrong;
    wrong["modules"] = QJsonArray{ QJsonObject(
      { { "type", "exploded" },
        { "node", pipeline.nodeId(slice) },
        { "start", 0 },
        { "stop", 1 },
        { "unit", "gap" } }) };
    animations.deserialize(wrong, &pipeline);
    QVERIFY(animations.animations().isEmpty());
  }

  // A threshold range end and a cut-out position sweep during playback
  // and come back from a state file, built through the property table
  // the Animation Helper authors with.
  void thresholdAndCutOutSweepsPlayAndSurviveAStateFile()
  {
    pipeline::Pipeline pipeline;
    auto* threshold = new pipeline::ThresholdSink();
    pipeline.addNode(threshold);
    threshold->setThresholdRange(10.0, 90.0);
    auto* volume = new pipeline::VolumeSink();
    pipeline.addNode(volume);
    QVERIFY(!volume->cutOutEnabled());

    const auto* lower = animatableProperty("thresholdLower");
    const auto* cutOutY = animatableProperty("cutOutY");
    QVERIFY(lower && cutOutY);
    QVERIFY(lower->applies(threshold) && !lower->applies(volume));
    QVERIFY(cutOutY->applies(volume) && !cutOutY->applies(threshold));
    // The lower end's default sweep grows the segmentation from where
    // the panel has it
    QCOMPARE(lower->range(threshold).start, 10.0);

    auto& animations = ModuleAnimations::instance();
    animations.add(lower->make(threshold, 10.0, 40.0));
    animations.add(cutOutY->make(volume, 0.0, 1.0));

    play(10);
    QVERIFY2(std::abs(threshold->lowerThreshold() - 40.0) < 3.0,
             qPrintable(QString("lower %1").arg(threshold->lowerThreshold())));
    QCOMPARE(threshold->upperThreshold(), 90.0);
    QVERIFY(volume->cutOutEnabled());
    QVERIFY(std::abs(volume->cutOutPosition(1) - 1.0) < 0.1);
    QCOMPARE(volume->cutOutPosition(0), 0.5);

    auto json = animations.serialize(&pipeline);
    animations.deserialize(json, &pipeline);
    auto restored = animations.animations();
    QCOMPARE(restored.size(), 2);
    double start = 0.0;
    double stop = 0.0;
    const auto* first = animatablePropertyOf(restored[0], start, stop);
    QVERIFY(first);
    QCOMPARE(first->id, QString("thresholdLower"));
    QCOMPARE(start, 10.0);
    QCOMPARE(stop, 40.0);
    const auto* second = animatablePropertyOf(restored[1], start, stop);
    QVERIFY(second);
    QCOMPARE(second->id, QString("cutOutY"));
    QCOMPARE(stop, 1.0);
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
  int status = QTest::qExec(&tc, argc, argv);
  RecordedAnimationTest recorded;
  status |= QTest::qExec(&recorded, argc, argv);
  return status;
}

#include "AnimationPlaybackTest.moc"
