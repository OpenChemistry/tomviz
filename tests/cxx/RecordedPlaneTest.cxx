/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QTest>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqServerResource.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkSMViewProxy.h>

#include "animations/CameraViewpoints.h"
#include "animations/RecordedAnimations.h"
#include "animations/SceneSnapshot.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/SliceSink.h"

#include <cmath>

using namespace tomviz;

namespace {

pipeline::SourceNode* addSource(pipeline::Pipeline* pip)
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(20, 30, 40);
  image->AllocateScalars(VTK_FLOAT, 1);
  auto* p = static_cast<float*>(image->GetScalarPointer());
  for (vtkIdType i = 0; i < image->GetNumberOfPoints(); ++i) {
    p[i] = static_cast<float>(i % 7);
  }
  auto vol = std::make_shared<pipeline::VolumeData>(
    vtkSmartPointer<vtkImageData>(image.GetPointer()));
  auto* source = new pipeline::SourceNode();
  source->addOutput("volume", pipeline::PortType::Volume);
  pip->addNode(source);
  source->setOutputData(
    "volume", pipeline::PortData(vol, pipeline::PortType::Volume));
  return source;
}

template <typename Sink>
Sink* addSink(pipeline::Pipeline* pip, pipeline::SourceNode* source,
              pqRenderView* view)
{
  auto* sink = new Sink();
  sink->initialize(view->getViewProxy());
  pip->addNode(sink);
  pip->createLink(source->outputPorts()[0], sink->inputPorts()[0]);
  sink->execute();
  return sink;
}

PlaneKey keyOf(pipeline::LegacyModuleSink* sink)
{
  auto snapshot = SinkSnapshot::capture(sink);
  PlaneKey key;
  key.direction = snapshot.planeDirection.value_or(0);
  key.slice = snapshot.sliceIndex.value_or(0);
  key.center = snapshot.planeCenter.value_or(key.center);
  key.normal = snapshot.planeNormal.value_or(key.normal);
  return key;
}

std::array<double, 3> unit(const double n[3])
{
  double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  return { n[0] / length, n[1] / length, n[2] / length };
}

} // namespace

class RecordedPlaneTest : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    auto* server = pqActiveObjects::instance().activeServer();
    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    m_view = qobject_cast<pqRenderView*>(
      builder->createView(pqRenderView::renderViewType(), server));
    QVERIFY(m_view);
    // Two viewpoints and one straight leg: path time is progress along it
    Viewpoint first;
    first.eased = false;
    Viewpoint second = first;
    second.position = { 0, 0, 5 };
    CameraViewpoints::instance().append(first);
    CameraViewpoints::instance().append(second);
  }

  // A slice that changes direction between viewpoints swings from one
  // orientation to the other through custom planes, taking the short way
  // round, and ends on the recorded direction and index.
  void sliceSwingsToItsNewDirection()
  {
    pipeline::Pipeline pip;
    auto* source = addSource(&pip);
    auto* slice = addSink<pipeline::SliceSink>(&pip, source, m_view);

    slice->setDirection(pipeline::SliceSink::XY);
    slice->setSlice(10);
    const auto xy = keyOf(slice);
    slice->setDirection(pipeline::SliceSink::XZ);
    slice->setSlice(5);
    const auto xz = keyOf(slice);

    RecordedPlaneAnimation animation(slice, { { 0, xy }, { 1, xz } });
    animation.applyPathTime(0.0);
    QCOMPARE(slice->direction(), pipeline::SliceSink::XY);
    QCOMPARE(slice->slice(), 10);

    animation.applyPathTime(0.5);
    QCOMPARE(slice->direction(), pipeline::SliceSink::Custom);
    double n[3];
    slice->planeNormal(n);
    auto half = unit(n);
    // Halfway between z and y, in the yz plane
    QVERIFY2(std::abs(half[0]) < 1e-6, "the normal left the yz plane");
    QVERIFY2(std::abs(std::abs(half[1]) - std::sqrt(0.5)) < 1e-6,
             "not halfway round");
    QVERIFY2(std::abs(std::abs(half[2]) - std::sqrt(0.5)) < 1e-6,
             "not halfway round");

    animation.applyPathTime(1.0);
    QCOMPARE(slice->direction(), pipeline::SliceSink::XZ);
    QCOMPARE(slice->slice(), 5);
  }

  // A clip swings the same way, keeping the side its normal points to.
  void clipSwingsToItsNewDirection()
  {
    pipeline::Pipeline pip;
    auto* source = addSource(&pip);
    auto* clip = addSink<pipeline::ClipSink>(&pip, source, m_view);

    clip->setDirection(pipeline::ClipSink::XY);
    const auto xy = keyOf(clip);
    clip->setDirection(pipeline::ClipSink::YZ);
    const auto yz = keyOf(clip);

    RecordedPlaneAnimation animation(clip, { { 0, xy }, { 1, yz } });
    animation.applyPathTime(0.5);
    QCOMPARE(clip->direction(), pipeline::ClipSink::Custom);
    double n[3];
    clip->planeNormalInData(n);
    auto half = unit(n);
    auto from = unit(xy.normal.data());
    auto to = unit(yz.normal.data());
    // The same angle from both ends, a quarter turn in all
    const double toStart =
      half[0] * from[0] + half[1] * from[1] + half[2] * from[2];
    const double toEnd = half[0] * to[0] + half[1] * to[1] + half[2] * to[2];
    QVERIFY2(std::abs(toStart - toEnd) < 1e-6, "not halfway round");
    QVERIFY2(std::abs(toStart - std::sqrt(0.5)) < 1e-6,
             "the clip turned the long way");

    animation.applyPathTime(1.0);
    QCOMPARE(clip->direction(), pipeline::ClipSink::YZ);
  }

  // The arrow belongs to the plane: with Show Plane off it stays hidden
  // when playback or Go To shows the clip again, and when a state file
  // turns Show Arrow on after Show Plane off.
  void aHiddenPlaneKeepsItsArrowHidden()
  {
    pipeline::Pipeline pip;
    auto* source = addSource(&pip);
    auto* clip = addSink<pipeline::ClipSink>(&pip, source, m_view);
    QVERIFY(clip->arrowVisible());

    clip->setShowPlane(false);
    QVERIFY(!clip->arrowVisible());
    clip->setShowArrow(true);
    QVERIFY(!clip->arrowVisible());
    clip->setVisibility(false);
    clip->setVisibility(true);
    QVERIFY2(!clip->arrowVisible(), "showing the clip again showed the arrow");

    clip->setShowPlane(true);
    QVERIFY(clip->arrowVisible());
    clip->setShowArrow(false);
    QVERIFY(!clip->arrowVisible());
  }

private:
  pqRenderView* m_view = nullptr;
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  RecordedPlaneTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "RecordedPlaneTest.moc"
