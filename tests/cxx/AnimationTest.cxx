/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "CameraViewpoints.h"
#include "ContourAnimation.h"
#include "ModuleAnimations.h"
#include "OpacityInterpolation.h"
#include "OpacityAnimation.h"
#include "SceneSnapshot.h"
#include "Pipeline.h"
#include "SliceAnimation.h"
#include "sinks/ClipSink.h"
#include "sinks/ContourSink.h"
#include "sinks/SliceSink.h"

#include <vtkCamera.h>
#include <vtkPiecewiseFunction.h>
#include <vtkNew.h>

#include <QJsonArray>

#include <cmath>

using tomviz::CameraViewpoints;
using tomviz::ContourAnimation;
using tomviz::interpolateOpacity;
using tomviz::ModuleAnimations;
using tomviz::OpacityAnimation;
using tomviz::SceneSnapshot;
using tomviz::applySceneTransition;
using tomviz::SliceAnimation;
using tomviz::Viewpoint;
using tomviz::pipeline::planeTravelRange;

namespace {

Viewpoint viewpointAt(double x, double duration, bool eased)
{
  Viewpoint viewpoint;
  viewpoint.position = { x, 0, 10 };
  viewpoint.focalPoint = { x, 0, 0 };
  viewpoint.duration = duration;
  viewpoint.eased = eased;
  return viewpoint;
}

// The viewpoint list is shared by the whole application, so each test
// starts from an empty one.
class AnimationTest : public ::testing::Test
{
protected:
  void SetUp() override { clearAll(); }
  void TearDown() override { clearAll(); }

  void clearAll()
  {
    CameraViewpoints::instance().clear();
    ModuleAnimations::instance().clear();
  }

  CameraViewpoints& viewpoints() { return CameraViewpoints::instance(); }
};

} // namespace

TEST_F(AnimationTest, PlaneTravelRangeCoversTheDataAlongTheNormal)
{
  double bounds[6] = { 0, 10, 0, 20, 0, 30 };

  double minDistance = 0;
  double maxDistance = 0;
  double z[3] = { 0, 0, 1 };
  planeTravelRange(bounds, z, minDistance, maxDistance);
  EXPECT_DOUBLE_EQ(minDistance, -15.0);
  EXPECT_DOUBLE_EQ(maxDistance, 15.0);

  // Along a diagonal the box is wider than along either of its axes, and
  // the normal does not have to arrive normalized.
  double diagonal[3] = { 3, 3, 0 };
  planeTravelRange(bounds, diagonal, minDistance, maxDistance);
  double expected = (5.0 + 10.0) / std::sqrt(2.0);
  EXPECT_NEAR(maxDistance, expected, 1e-9);
  EXPECT_NEAR(minDistance, -expected, 1e-9);

  // A plane with no normal has nowhere to travel.
  double degenerate[3] = { 0, 0, 0 };
  planeTravelRange(bounds, degenerate, minDistance, maxDistance);
  EXPECT_DOUBLE_EQ(minDistance, 0.0);
  EXPECT_DOUBLE_EQ(maxDistance, 0.0);
}

TEST_F(AnimationTest, SegmentDurationsDivideUpTheAnimation)
{
  EXPECT_TRUE(viewpoints().stops().isEmpty());

  viewpoints().append(viewpointAt(0, 1.0, false));
  EXPECT_TRUE(viewpoints().stops().isEmpty()) << "one viewpoint is not a path";

  viewpoints().append(viewpointAt(1, 3.0, false));
  viewpoints().append(viewpointAt(2, 1.0, false));

  // The first leg is a quarter as long as the second, so it is over a
  // quarter of the way through.
  auto stops = viewpoints().stops();
  ASSERT_EQ(stops.size(), 3);
  EXPECT_DOUBLE_EQ(stops[0], 0.0);
  EXPECT_DOUBLE_EQ(stops[1], 0.25);
  EXPECT_DOUBLE_EQ(stops[2], 1.0);

  // Durations that leave no time at all fall back to equal legs.
  viewpoints().replace(0, viewpointAt(0, 0.0, false));
  viewpoints().replace(1, viewpointAt(1, 0.0, false));
  stops = viewpoints().stops();
  ASSERT_EQ(stops.size(), 3);
  EXPECT_DOUBLE_EQ(stops[1], 0.5);
}

TEST_F(AnimationTest, RemappingProgressLeavesTheViewpointsWhereTheyAre)
{
  viewpoints().append(viewpointAt(0, 3.0, true));
  viewpoints().append(viewpointAt(1, 1.0, true));
  viewpoints().append(viewpointAt(2, 1.0, true));

  // Easing changes the pacing inside a leg, never when a leg ends.
  for (auto stop : viewpoints().stops()) {
    EXPECT_NEAR(viewpoints().remapProgress(stop), stop, 1e-12);
  }

  double previous = -1;
  for (int i = 0; i <= 100; ++i) {
    double remapped = viewpoints().remapProgress(i / 100.0);
    EXPECT_GE(remapped, previous) << "time ran backwards at " << i;
    EXPECT_GE(remapped, 0.0);
    EXPECT_LE(remapped, 1.0);
    previous = remapped;
  }

  // Out-of-range progress clamps rather than running off the path.
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(-0.5), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(1.5), 1.0);
}

TEST_F(AnimationTest, EasingSlowsTheEndsOfALegAndNotItsMiddle)
{
  viewpoints().append(viewpointAt(0, 1.0, false));
  viewpoints().append(viewpointAt(1, 1.0, false));

  // Without easing the camera moves at a constant rate.
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(0.25), 0.25);
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(0.75), 0.75);

  viewpoints().replace(0, viewpointAt(0, 1.0, true));

  // With it, the camera has covered less of the leg by the first quarter
  // and more by the last, and the two ends stay symmetric.
  double quarter = viewpoints().remapProgress(0.25);
  double threeQuarters = viewpoints().remapProgress(0.75);
  EXPECT_LT(quarter, 0.25);
  EXPECT_GT(threeQuarters, 0.75);
  EXPECT_NEAR(quarter, 1.0 - threeQuarters, 1e-12);
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(0.5), 0.5);
}

TEST_F(AnimationTest, BindingToALegConfinesAnAnimationToIt)
{
  viewpoints().append(viewpointAt(0, 1.0, false));
  viewpoints().append(viewpointAt(1, 3.0, false));
  viewpoints().append(viewpointAt(2, 1.0, false));
  // Legs run 0 -> 0.25 -> 1.

  // The first leg: done by the time the camera reaches viewpoint 2, and
  // held at either end rather than running on.
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.0, 0), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.125, 0), 0.5);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.25, 0), 1.0);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.9, 0), 1.0);

  // The second leg has not started while the first one is running.
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.1, 1), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.625, 1), 0.5);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(1.0, 1), 1.0);

  // An animation bound to a leg that is not there any more runs over the
  // whole timeline instead of freezing.
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.4, 7), 0.4);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.4, -1), 0.4);
}

TEST_F(AnimationTest, ThePathPassesThroughEveryViewpoint)
{
  viewpoints().append(viewpointAt(0, 1.0, true));
  viewpoints().append(viewpointAt(5, 3.0, true));
  viewpoints().append(viewpointAt(9, 1.0, true));

  auto stops = viewpoints().stops();
  ASSERT_EQ(stops.size(), 3);

  vtkNew<vtkCamera> camera;
  for (int i = 0; i < 3; ++i) {
    viewpoints().interpolate(stops[i], camera);
    double position[3];
    camera->GetPosition(position);
    EXPECT_NEAR(position[0], viewpoints().at(i).position[0], 1e-6)
      << "viewpoint " << i << " was not reached";
  }
}

TEST_F(AnimationTest, ViewpointsSurviveAStateFileRoundTrip)
{
  Viewpoint saved;
  saved.position = { 1, 2, 3 };
  saved.focalPoint = { 4, 5, 6 };
  saved.viewUp = { 0, 0, 1 };
  saved.viewAngle = 45;
  saved.parallelScale = 2.5;
  saved.parallelProjection = true;
  saved.duration = 2.5;
  saved.eased = false;
  saved.name = "Money shot";
  saved.thumbnail = QByteArray("not really a png, but it should come back");

  viewpoints().append(saved);
  viewpoints().append(viewpointAt(7, 1.0, true));
  auto json = viewpoints().serialize();

  viewpoints().clear();
  ASSERT_TRUE(viewpoints().deserialize(json));
  ASSERT_EQ(viewpoints().size(), 2);

  const auto& restored = viewpoints().at(0);
  EXPECT_EQ(restored.position, saved.position);
  EXPECT_EQ(restored.focalPoint, saved.focalPoint);
  EXPECT_EQ(restored.viewUp, saved.viewUp);
  EXPECT_DOUBLE_EQ(restored.viewAngle, saved.viewAngle);
  EXPECT_DOUBLE_EQ(restored.parallelScale, saved.parallelScale);
  EXPECT_TRUE(restored.parallelProjection);
  EXPECT_DOUBLE_EQ(restored.duration, saved.duration);
  EXPECT_FALSE(restored.eased);
  EXPECT_EQ(restored.thumbnail, saved.thumbnail);
  EXPECT_EQ(restored.name, saved.name);
  // The second viewpoint was saved without a name, as older files were,
  // and gets a positional one rather than none.
  EXPECT_EQ(viewpoints().at(1).name, QString("Viewpoint 2"));

  // A state file with no viewpoints in it is not a list of none.
  EXPECT_FALSE(viewpoints().deserialize(QJsonObject()));
}

// Building a live animation needs a running ParaView application (the
// base class reaches ActiveObjects for the time keeper), so what is
// checked here is the half that does not: an entry is only rebuilt if
// the state file still describes something that can carry it.
TEST_F(AnimationTest, SavedAnimationsWithoutTheirVisualizationAreDropped)
{
  tomviz::pipeline::Pipeline pipeline;
  auto* contour = new tomviz::pipeline::ContourSink();
  pipeline.addNode(contour);
  int contourId = pipeline.nodeId(contour);

  QJsonArray entries;
  // A visualization that is not in this state file.
  entries.append(QJsonObject({ { "type", "contour" },
                               { "node", contourId + 100 },
                               { "start", 0 },
                               { "stop", 1 } }));
  // A kind of animation this build does not have.
  entries.append(QJsonObject({ { "type", "hologram" },
                               { "node", contourId },
                               { "start", 0 },
                               { "stop", 1 } }));
  // An animation on a visualization of the wrong type.
  entries.append(QJsonObject({ { "type", "clip" },
                               { "node", contourId },
                               { "start", 0 },
                               { "stop", 1 },
                               { "unit", "slice" } }));

  QJsonObject json;
  json["modules"] = entries;

  auto& animations = ModuleAnimations::instance();
  animations.deserialize(json, &pipeline);
  EXPECT_TRUE(animations.animations().isEmpty());

  // A state file with no animation section leaves nothing behind.
  animations.deserialize(QJsonObject(), &pipeline);
  EXPECT_TRUE(animations.animations().isEmpty());
  EXPECT_TRUE(animations.serialize(&pipeline)["modules"].toArray().isEmpty());
}

TEST_F(AnimationTest, DefaultViewpointNamesAreNotReused)
{
  auto first = viewpointAt(0, 1.0, true);
  first.name = viewpoints().nextDefaultName();
  viewpoints().append(first);
  EXPECT_EQ(first.name, QString("Viewpoint 1"));

  auto second = viewpointAt(1, 1.0, true);
  second.name = viewpoints().nextDefaultName();
  viewpoints().append(second);
  EXPECT_EQ(second.name, QString("Viewpoint 2"));

  // Deleting the first viewpoint must not hand its name to the next one:
  // anything referring to "Viewpoint 1" would silently mean a different
  // camera position.
  viewpoints().removeAt(0);
  EXPECT_EQ(viewpoints().nextDefaultName(), QString("Viewpoint 3"));

  // A rename frees nothing; numbering keys off the names still in use.
  auto renamed = viewpoints().at(0);
  renamed.name = "Money shot";
  viewpoints().replace(0, renamed);
  EXPECT_EQ(viewpoints().nextDefaultName(), QString("Viewpoint 1"));
}

TEST_F(AnimationTest, CurveAnchorsResolveThroughTheCameraPath)
{
  // With no path, anchors still span the animation, so a morph keyed to
  // them behaves like a plain start-to-end morph.
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(0), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(1), 1.0);
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(5), 1.0);

  viewpoints().append(viewpointAt(0, 1.0, false));
  viewpoints().append(viewpointAt(1, 3.0, false));
  viewpoints().append(viewpointAt(2, 1.0, false));

  // Anchored curves land when the camera does, so they follow the same
  // stops that pace it, retiming and all.
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(0), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(1), 0.25);
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(2), 1.0);

  // Anchors to viewpoints that no longer exist clamp instead of running
  // off the path.
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(9), 1.0);
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(-3), 0.0);
}

namespace {

vtkSmartPointer<vtkPiecewiseFunction> curve(
  std::initializer_list<std::array<double, 2>> points)
{
  auto function = vtkSmartPointer<vtkPiecewiseFunction>::New();
  for (const auto& point : points) {
    function->AddPoint(point[0], point[1]);
  }
  return function;
}

} // namespace

TEST_F(AnimationTest, BlendingOpacityCurvesReproducesItsEndpoints)
{
  auto from = curve({ { 0, 0 }, { 40, 1 }, { 100, 0 } });
  auto sparse = curve({ { 0, 0 }, { 100, 1 } });
  double range[2] = { 0, 100 };

  auto out = vtkSmartPointer<vtkPiecewiseFunction>::New();

  // Matched point counts blend point by point, so the ends come back exactly.
  auto matched = curve({ { 0, 0.2 }, { 60, 0.5 }, { 100, 0.9 } });
  interpolateOpacity(from, matched, 0.0, range, out);
  EXPECT_EQ(out->GetSize(), 3);
  EXPECT_DOUBLE_EQ(out->GetValue(40), 1.0);
  interpolateOpacity(from, matched, 1.0, range, out);
  EXPECT_DOUBLE_EQ(out->GetValue(60), 0.5);

  // Mismatched counts go through a table, so the ends come back to within
  // the sampling resolution rather than exactly.
  interpolateOpacity(from, sparse, 0.0, range, out);
  EXPECT_NEAR(out->GetValue(40), 1.0, 0.02);
  interpolateOpacity(from, sparse, 1.0, range, out);
  EXPECT_NEAR(out->GetValue(50), 0.5, 0.02);

  // Out of range values clamp rather than extrapolating off either end.
  interpolateOpacity(from, matched, 4.0, range, out);
  EXPECT_DOUBLE_EQ(out->GetValue(60), 0.5);
}

TEST_F(AnimationTest, BlendingKeepsCurvesEditableWhenItCan)
{
  double range[2] = { 0, 100 };
  auto out = vtkSmartPointer<vtkPiecewiseFunction>::New();

  // Same number of points: the result stays as small as its inputs, and
  // sharpness carries across instead of being baked into samples.
  auto from = vtkSmartPointer<vtkPiecewiseFunction>::New();
  from->AddPoint(0, 0.0, 0.5, 1.0);
  from->AddPoint(100, 1.0, 0.5, 1.0);
  auto to = vtkSmartPointer<vtkPiecewiseFunction>::New();
  to->AddPoint(0, 0.0, 0.5, 0.0);
  to->AddPoint(100, 1.0, 0.5, 0.0);

  interpolateOpacity(from, to, 0.5, range, out);
  ASSERT_EQ(out->GetSize(), 2);
  double node[4];
  out->GetNodeValue(0, node);
  EXPECT_DOUBLE_EQ(node[3], 0.5) << "sharpness should blend, not reset";

  // Different counts cannot be paired up, so the result is a sampled curve.
  auto three = curve({ { 0, 0 }, { 50, 1 }, { 100, 0 } });
  interpolateOpacity(from, three, 0.5, range, out);
  EXPECT_GT(out->GetSize(), 3);
}

TEST_F(AnimationTest, BlendingReadsBothCurvesOverTheSameWindow)
{
  // The same shape stored over two different data windows. Blended over one
  // reference window they agree, so the midpoint matches the endpoints
  // instead of sliding sideways.
  auto narrow = curve({ { 0, 0 }, { 10, 1 }, { 20, 0 } });
  auto wide = curve({ { 0, 0 }, { 50, 1 }, { 100, 0 } });
  double range[2] = { 0, 100 };

  auto out = vtkSmartPointer<vtkPiecewiseFunction>::New();
  interpolateOpacity(narrow, wide, 0.5, range, out);

  // Halfway between a peak at 10 and one at 50 is a peak at 30.
  double node[4];
  out->GetNodeValue(1, node);
  EXPECT_DOUBLE_EQ(node[0], 30.0);
  EXPECT_DOUBLE_EQ(node[1], 1.0);
}

TEST_F(AnimationTest, BlendingSurvivesEmptyAndDegenerateInput)
{
  double range[2] = { 0, 100 };
  auto out = vtkSmartPointer<vtkPiecewiseFunction>::New();
  auto real = curve({ { 0, 0 }, { 100, 1 } });
  auto empty = vtkSmartPointer<vtkPiecewiseFunction>::New();

  // Nothing to blend towards: the curve that exists stands for the whole
  // animation rather than fading to nothing.
  interpolateOpacity(real, empty, 0.5, range, out);
  EXPECT_DOUBLE_EQ(out->GetValue(100), 1.0);
  interpolateOpacity(empty, real, 0.5, range, out);
  EXPECT_DOUBLE_EQ(out->GetValue(100), 1.0);

  interpolateOpacity(empty, empty, 0.5, range, out);
  EXPECT_EQ(out->GetSize(), 0);

  // A collapsed window has nothing to sample over.
  double collapsed[2] = { 5, 5 };
  auto three = curve({ { 0, 0 }, { 50, 1 }, { 100, 0 } });
  interpolateOpacity(real, three, 0.9, collapsed, out);
  EXPECT_EQ(out->GetSize(), 3) << "should hold the nearer curve";

  // Writing back over an input must not read freed or half-written state.
  interpolateOpacity(real, three, 0.5, range, real);
  EXPECT_GT(real->GetSize(), 0);
}

TEST_F(AnimationTest, RecordedModuleStateSurvivesAStateFileRoundTrip)
{
  tomviz::pipeline::Pipeline pipeline;
  auto* slice = new tomviz::pipeline::SliceSink();
  pipeline.addNode(slice);
  slice->setOpacity(0.3);
  slice->setVisibility(false);

  Viewpoint viewpoint;
  viewpoint.label = "Cu channel";
  viewpoint.scene = SceneSnapshot::capture(&pipeline);
  auto restored = Viewpoint::deserialize(viewpoint.serialize());
  EXPECT_EQ(restored.label, "Cu channel");

  int id = pipeline.nodeId(slice);
  ASSERT_TRUE(restored.scene.sinks.contains(id));
  EXPECT_FALSE(restored.scene.sinks[id].visible);
  ASSERT_TRUE(restored.scene.sinks[id].opacity.has_value());
  EXPECT_DOUBLE_EQ(*restored.scene.sinks[id].opacity, 0.3);
}

TEST_F(AnimationTest, SceneTransitionsFadeWhatChangedAndHoldTheRest)
{
  tomviz::pipeline::Pipeline pipeline;
  auto* steady = new tomviz::pipeline::SliceSink();
  auto* fading = new tomviz::pipeline::SliceSink();
  pipeline.addNode(steady);
  pipeline.addNode(fading);
  steady->setOpacity(1.0);
  fading->setOpacity(0.8);

  auto from = SceneSnapshot::capture(&pipeline);
  fading->setVisibility(false);
  auto to = SceneSnapshot::capture(&pipeline);

  // A module identical at both ends is never written, so an edit made
  // after recording stays put
  steady->setOpacity(0.4);
  QSet<int> overridden;
  applySceneTransition(&pipeline, from, to, 0.5, overridden);
  EXPECT_DOUBLE_EQ(steady->opacity(), 0.4);

  // The disappearing module fades out and only hides at the end
  EXPECT_TRUE(fading->visibility());
  EXPECT_DOUBLE_EQ(fading->opacity(), 0.4);
  applySceneTransition(&pipeline, from, to, 1.0, overridden);
  EXPECT_FALSE(fading->visibility());
  EXPECT_DOUBLE_EQ(fading->opacity(), 0.0);
  applySceneTransition(&pipeline, from, to, 0.0, overridden);
  EXPECT_TRUE(fading->visibility());
  EXPECT_DOUBLE_EQ(fading->opacity(), 0.8);
}
