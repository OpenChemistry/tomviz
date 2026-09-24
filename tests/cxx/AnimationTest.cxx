/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "CameraViewpoints.h"
#include "ContourAnimation.h"
#include "ModuleAnimations.h"
#include "OpacityInterpolation.h"
#include "OpacityAnimation.h"
#include "RecordedAnimations.h"
#include "SceneSnapshot.h"
#include "Pipeline.h"
#include "SliceAnimation.h"
#include "sinks/ClipSink.h"
#include "sinks/ContourSink.h"
#include "sinks/SliceSink.h"
#include "sinks/ThresholdSink.h"

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
using tomviz::RecordedAnimations;
using tomviz::SliceAnimation;
using tomviz::SinkSnapshot;
using tomviz::Viewpoint;
using tomviz::pipeline::planeTravelRange;

namespace {

Viewpoint viewpointAt(double x, int legFrames, bool eased)
{
  Viewpoint viewpoint;
  viewpoint.position = { x, 0, 10 };
  viewpoint.focalPoint = { x, 0, 0 };
  viewpoint.legFrames = legFrames;
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

  viewpoints().append(viewpointAt(0, 1, false));
  EXPECT_TRUE(viewpoints().stops().isEmpty()) << "one viewpoint is not a path";

  viewpoints().append(viewpointAt(1, 3, false));
  viewpoints().append(viewpointAt(2, 1, false));

  // The first leg is a quarter as long as the second, so it is over a
  // quarter of the way through.
  auto stops = viewpoints().stops();
  ASSERT_EQ(stops.size(), 3);
  EXPECT_DOUBLE_EQ(stops[0], 0.0);
  EXPECT_DOUBLE_EQ(stops[1], 0.25);
  EXPECT_DOUBLE_EQ(stops[2], 1.0);

  // Durations that leave no time at all fall back to equal legs.
  viewpoints().replace(0, viewpointAt(0, 0, false));
  viewpoints().replace(1, viewpointAt(1, 0, false));
  stops = viewpoints().stops();
  ASSERT_EQ(stops.size(), 3);
  EXPECT_DOUBLE_EQ(stops[1], 0.5);
}

TEST_F(AnimationTest, RemappingProgressLeavesTheViewpointsWhereTheyAre)
{
  viewpoints().append(viewpointAt(0, 3, true));
  viewpoints().append(viewpointAt(1, 1, true));
  viewpoints().append(viewpointAt(2, 1, true));

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
  viewpoints().append(viewpointAt(0, 1, false));
  viewpoints().append(viewpointAt(1, 1, false));

  // Without easing the camera moves at a constant rate.
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(0.25), 0.25);
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(0.75), 0.75);

  viewpoints().replace(0, viewpointAt(0, 1, true));

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
  viewpoints().append(viewpointAt(0, 1, false));
  viewpoints().append(viewpointAt(1, 3, false));
  viewpoints().append(viewpointAt(2, 1, false));
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
  viewpoints().append(viewpointAt(0, 1, true));
  viewpoints().append(viewpointAt(5, 3, true));
  viewpoints().append(viewpointAt(9, 1, true));

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

namespace {

Viewpoint lookingAtOrigin(const std::array<double, 3>& position, int turns)
{
  Viewpoint viewpoint;
  viewpoint.position = position;
  viewpoint.focalPoint = { 0, 0, 0 };
  viewpoint.viewUp = { 0, 1, 0 };
  viewpoint.orbitTurns = turns;
  viewpoint.eased = false;
  // Equal shares for legs and orbits, so the stops below come out round
  viewpoint.legFrames = 1;
  viewpoint.orbitFrames = 1;
  return viewpoint;
}

std::array<double, 3> positionAt(double t)
{
  vtkNew<vtkCamera> camera;
  CameraViewpoints::instance().interpolate(t, camera);
  std::array<double, 3> position;
  camera->GetPosition(position.data());
  return position;
}

void expectNear(const std::array<double, 3>& a, const std::array<double, 3>& b,
                const char* what)
{
  for (int k = 0; k < 3; ++k) {
    EXPECT_NEAR(a[k], b[k], 1e-6) << what << " component " << k;
  }
}

} // namespace

// A viewpoint that orbits spins the camera once around the focal point
// on the view-up axis, at the same distance all the way, and hands it
// back where it started. Alone, it is an animation by itself.
TEST_F(AnimationTest, AnOrbitingViewpointSpinsInPlace)
{
  viewpoints().append(lookingAtOrigin({ 0, 0, 10 }, 1));
  EXPECT_TRUE(viewpoints().isPath());
  ASSERT_EQ(viewpoints().stops().size(), 1);
  EXPECT_DOUBLE_EQ(viewpoints().departures()[0], 1.0);

  expectNear(positionAt(0.0), { 0, 0, 10 }, "start");
  // Counterclockwise seen from above (+y): the camera goes to +x first
  expectNear(positionAt(0.25), { 10, 0, 0 }, "quarter turn");
  expectNear(positionAt(0.5), { 0, 0, -10 }, "half turn");
  expectNear(positionAt(0.75), { -10, 0, 0 }, "three quarters");
  expectNear(positionAt(1.0), { 0, 0, 10 }, "full turn");
  for (int i = 0; i <= 20; ++i) {
    auto p = positionAt(i / 20.0);
    EXPECT_NEAR(std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]), 10.0,
                1e-6)
      << "distance changed at " << i;
    EXPECT_NEAR(p[1], 0.0, 1e-6) << "left the orbit plane at " << i;
  }

  // Clockwise goes the other way, and two turns pass the far side twice
  viewpoints().replace(0, lookingAtOrigin({ 0, 0, 10 }, -1));
  expectNear(positionAt(0.25), { -10, 0, 0 }, "clockwise quarter");
  viewpoints().replace(0, lookingAtOrigin({ 0, 0, 10 }, 2));
  expectNear(positionAt(0.25), { 0, 0, -10 }, "two turns, first far side");
  expectNear(positionAt(0.5), { 0, 0, 10 }, "two turns, halfway");

  // Without the orbit a lone viewpoint is no path at all
  viewpoints().replace(0, lookingAtOrigin({ 0, 0, 10 }, 0));
  EXPECT_FALSE(viewpoints().isPath());
  EXPECT_TRUE(viewpoints().stops().isEmpty());
}

// An orbit takes its own share of the timeline: the camera arrives,
// spins, then flies on. Whatever is bound to the leg waits for the spin
// to finish, and a curve keyed to the viewpoint holds through it.
TEST_F(AnimationTest, AnOrbitHoldsTheLegAndTheAnchorsStill)
{
  auto first = lookingAtOrigin({ 0, 0, 10 }, 1);
  first.legFrames = 1;
  first.orbitFrames = 1;
  viewpoints().append(first);
  viewpoints().append(lookingAtOrigin({ 0, 0, -10 }, 0));

  // Orbit 0 to 0.5, leg 0.5 to 1
  auto stops = viewpoints().stops();
  auto departures = viewpoints().departures();
  ASSERT_EQ(stops.size(), 2);
  EXPECT_DOUBLE_EQ(stops[0], 0.0);
  EXPECT_DOUBLE_EQ(departures[0], 0.5);
  EXPECT_DOUBLE_EQ(stops[1], 1.0);
  EXPECT_DOUBLE_EQ(departures[1], 1.0);

  expectNear(positionAt(0.125), { 10, 0, 0 }, "quarter turn");
  expectNear(positionAt(0.5), { 0, 0, 10 }, "back at the start");
  expectNear(positionAt(0.75), { 0, 0, 0 }, "halfway along the leg");
  expectNear(positionAt(1.0), { 0, 0, -10 }, "arrived");

  // The leg's own progress and the anchor spans skip the spin
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.25, 0), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.5, 0), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(0.75, 0), 0.5);
  EXPECT_DOUBLE_EQ(viewpoints().segmentProgress(1.0, 0), 1.0);
  EXPECT_DOUBLE_EQ(viewpoints().anchorTime(0), 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().departureTime(0), 0.5);
  QList<int> anchors = { 0, 1 };
  EXPECT_DOUBLE_EQ(tomviz::anchorSpanAt(anchors, 0.25).u, 0.0);
  EXPECT_DOUBLE_EQ(tomviz::anchorSpanAt(anchors, 0.75).u, 0.5);

  // A longer orbit takes a bigger share
  first.orbitFrames = 3;
  viewpoints().replace(0, first);
  EXPECT_DOUBLE_EQ(viewpoints().departures()[0], 0.75);
}

// An orbit spins at constant speed unless its own easing is on; the
// leg's easing does not reach it. A file from before orbits had their
// own easing plays them as the leg's easing did.
TEST_F(AnimationTest, OrbitsEaseOnlyWhenAskedTo)
{
  auto spinning = lookingAtOrigin({ 0, 0, 10 }, 1);
  spinning.eased = true;
  viewpoints().append(spinning);
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(0.25), 0.25);
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(0.75), 0.75);

  spinning.orbitEased = true;
  viewpoints().replace(0, spinning);
  double quarter = viewpoints().remapProgress(0.25);
  EXPECT_LT(quarter, 0.25);
  EXPECT_NEAR(quarter, 1.0 - viewpoints().remapProgress(0.75), 1e-12);
  EXPECT_DOUBLE_EQ(viewpoints().remapProgress(1.0), 1.0);

  spinning.orbitEased = false;
  viewpoints().replace(0, spinning);
  auto json = viewpoints().serialize();
  auto list = json["viewpoints"].toArray();
  auto old = list[0].toObject();
  old.remove("orbitEased");
  list[0] = old;
  json["viewpoints"] = list;
  ASSERT_TRUE(viewpoints().deserialize(json));
  EXPECT_TRUE(viewpoints().at(0).orbitEased);

  // A viewpoint with no orbit, from such a file or a new one, gets the
  // default rather than the leg's easing
  auto still = lookingAtOrigin({ 0, 0, 10 }, 0);
  still.eased = true;
  viewpoints().replace(0, still);
  json = viewpoints().serialize();
  ASSERT_TRUE(viewpoints().deserialize(json));
  EXPECT_FALSE(viewpoints().at(0).orbitEased);
  list = json["viewpoints"].toArray();
  old = list[0].toObject();
  old.remove("orbitEased");
  list[0] = old;
  json["viewpoints"] = list;
  ASSERT_TRUE(viewpoints().deserialize(json));
  EXPECT_FALSE(viewpoints().at(0).orbitEased);
}

// The last viewpoint can orbit, ending the animation on a spin, and a
// spin in the middle of a path leaves the legs either side straight.
TEST_F(AnimationTest, OrbitsAtTheEndAndInTheMiddleOfAPath)
{
  viewpoints().append(lookingAtOrigin({ 0, 0, 10 }, 0));
  viewpoints().append(lookingAtOrigin({ 0, 0, -10 }, 1));
  // Leg 0 to 0.5, orbit at the end 0.5 to 1
  expectNear(positionAt(0.25), { 0, 0, 0 }, "halfway along the leg");
  expectNear(positionAt(0.5), { 0, 0, -10 }, "arrived");
  // At the far side the camera's own e1 points to -z, so a quarter turn
  // counterclockwise takes it to -x
  expectNear(positionAt(0.625), { -10, 0, 0 }, "quarter turn at the end");
  expectNear(positionAt(1.0), { 0, 0, -10 }, "ends where it stopped");

  viewpoints().append(lookingAtOrigin({ 20, 0, -10 }, 0));
  // Leg 0 to 1/3, orbit 1/3 to 2/3, leg 2/3 to 1
  auto stops = viewpoints().stops();
  ASSERT_EQ(stops.size(), 3);
  EXPECT_NEAR(stops[1], 1.0 / 3.0, 1e-12);
  EXPECT_NEAR(viewpoints().departures()[1], 2.0 / 3.0, 1e-12);
  expectNear(positionAt(1.0 / 6.0), { 0, 0, 0 }, "first leg midpoint");
  expectNear(positionAt(0.5), { 0, 0, 10 }, "half turn in the middle");
  expectNear(positionAt(2.0 / 3.0), { 0, 0, -10 }, "spin over");
  expectNear(positionAt(5.0 / 6.0), { 10, 0, -10 }, "second leg midpoint");
}

// A camera looking straight down its own up axis has no radius to
// swing; the orbit holds it in place rather than producing NaNs.
TEST_F(AnimationTest, AnOrbitOnTheAxisHoldsStill)
{
  viewpoints().append(lookingAtOrigin({ 0, 10, 0 }, 1));
  auto p = positionAt(0.5);
  for (int k = 0; k < 3; ++k) {
    EXPECT_FALSE(std::isnan(p[k]));
  }
  expectNear(p, { 0, 10, 0 }, "stays put");
}

// Legs of no length are cuts. The path still has a start and an end to
// play, and a run whose cameras all sit at one time has nothing for a
// spline to curve through, which must not come out as garbage.
TEST_F(AnimationTest, LegsOfNoLengthAreCuts)
{
  viewpoints().append(viewpointAt(0, 0, false));
  viewpoints().append(viewpointAt(1, 0, false));
  viewpoints().append(viewpointAt(2, 0, false));
  EXPECT_EQ(viewpoints().totalFrames(), 2);
  for (double t : { 0.0, 0.3, 0.5, 0.7, 1.0 }) {
    auto p = positionAt(t);
    for (int k = 0; k < 3; ++k) {
      EXPECT_FALSE(std::isnan(p[k])) << "t = " << t;
    }
    EXPECT_GE(p[0], 0.0) << "t = " << t;
    EXPECT_LE(p[0], 2.0) << "t = " << t;
  }
  expectNear(positionAt(1.0), { 2, 0, 10 }, "ends at the last viewpoint");

  // One cut among real legs: the path jumps there and flies the rest
  viewpoints().clear();
  viewpoints().append(viewpointAt(0, 30, false));
  viewpoints().append(viewpointAt(1, 0, false));
  viewpoints().append(viewpointAt(2, 30, false));
  viewpoints().append(viewpointAt(3, 30, false));
  EXPECT_EQ(viewpoints().totalFrames(), 30 + 0 + 30);
  expectNear(positionAt(0.0), { 0, 0, 10 }, "start");
  expectNear(positionAt(1.0), { 3, 0, 10 }, "end");
  for (double t : { 0.1, 1.0 / 3.0, 0.5, 0.9 }) {
    auto p = positionAt(t);
    for (int k = 0; k < 3; ++k) {
      EXPECT_FALSE(std::isnan(p[k])) << "t = " << t;
    }
  }
}

// The animation is as long as the path: legs and orbits added up.
TEST_F(AnimationTest, ThePathSetsTheFrameCount)
{
  EXPECT_EQ(viewpoints().totalFrames(), 0);
  viewpoints().append(viewpointAt(0, 40, false));
  EXPECT_EQ(viewpoints().totalFrames(), 0) << "one plain viewpoint is no path";
  viewpoints().append(viewpointAt(1, 25, false));
  EXPECT_EQ(viewpoints().totalFrames(), 40) << "the last leg count is unused";
  auto spinning = viewpointAt(2, 7, false);
  spinning.orbitTurns = 1;
  spinning.orbitFrames = 30;
  viewpoints().append(spinning);
  EXPECT_EQ(viewpoints().totalFrames(), 40 + 25 + 30);
  // A lone orbit is an animation of its own length
  viewpoints().clear();
  viewpoints().append(spinning);
  EXPECT_EQ(viewpoints().totalFrames(), 30);
}

// A file from before legs had frame counts carried relative durations
// and one total; each piece gets its share of that total.
TEST_F(AnimationTest, LegacyDurationsBecomeFrameShares)
{
  QJsonObject a;
  a["duration"] = 1.0;
  QJsonObject b;
  b["duration"] = 3.0;
  b["orbitTurns"] = 1;
  b["orbitDuration"] = 2.0;
  QJsonObject c;
  QJsonObject file;
  file["viewpoints"] = QJsonArray{ a, b, c };
  file["numberOfFrames"] = 120;
  ASSERT_TRUE(viewpoints().deserialize(file));
  // Weights 1 + 3 + 2 = 6 over 120 frames
  EXPECT_EQ(viewpoints().at(0).legFrames, 20);
  EXPECT_EQ(viewpoints().at(1).legFrames, 60);
  EXPECT_EQ(viewpoints().at(1).orbitFrames, 40);
  EXPECT_EQ(viewpoints().totalFrames(), 120);

  // A current file is taken as written
  QJsonObject d;
  d["legFrames"] = 15;
  QJsonObject e;
  e["legFrames"] = 99;
  QJsonObject current;
  current["viewpoints"] = QJsonArray{ d, e };
  current["numberOfFrames"] = 1000;
  ASSERT_TRUE(viewpoints().deserialize(current));
  EXPECT_EQ(viewpoints().at(0).legFrames, 15);
  EXPECT_EQ(viewpoints().totalFrames(), 15);
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
  saved.legFrames = 25;
  saved.eased = false;
  saved.orbitTurns = -2;
  saved.orbitFrames = 50;
  saved.orbitEased = true;
  saved.name = "Money shot";
  saved.thumbnail = QByteArray("not really a png, but it should come back");

  viewpoints().append(saved);
  viewpoints().append(viewpointAt(7, 1, true));
  viewpoints().setCaptionPosition(0.6, 0.9);
  auto json = viewpoints().serialize();

  viewpoints().clear();
  viewpoints().setCaptionPosition(0.02, 0.03);
  ASSERT_TRUE(viewpoints().deserialize(json));
  ASSERT_EQ(viewpoints().size(), 2);
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[0], 0.6);
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[1], 0.9);

  const auto& restored = viewpoints().at(0);
  EXPECT_EQ(restored.position, saved.position);
  EXPECT_EQ(restored.focalPoint, saved.focalPoint);
  EXPECT_EQ(restored.viewUp, saved.viewUp);
  EXPECT_DOUBLE_EQ(restored.viewAngle, saved.viewAngle);
  EXPECT_DOUBLE_EQ(restored.parallelScale, saved.parallelScale);
  EXPECT_TRUE(restored.parallelProjection);
  EXPECT_EQ(restored.legFrames, 25);
  EXPECT_FALSE(restored.eased);
  EXPECT_EQ(restored.orbitTurns, -2);
  EXPECT_EQ(restored.orbitFrames, 50);
  EXPECT_TRUE(restored.orbitEased) << "an orbit's easing is its own";
  EXPECT_EQ(viewpoints().at(1).orbitTurns, 0) << "absent means no orbit";
  EXPECT_EQ(restored.thumbnail, saved.thumbnail);
  EXPECT_EQ(restored.name, saved.name);
  // The second viewpoint was saved without a name, as older files were,
  // and gets a positional one rather than none.
  EXPECT_EQ(viewpoints().at(1).name, QString("Viewpoint 2"));

  // A file from before captions could be moved centers them at the
  // bottom middle, as does one saved with the old corner default, which
  // would cut a centered caption in half; and the position never leaves
  // the view.
  json.remove("captionPosition");
  ASSERT_TRUE(viewpoints().deserialize(json));
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[0], 0.5);
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[1], 0.05);
  json["captionPosition"] = QJsonArray{ 0.02, 0.03 };
  ASSERT_TRUE(viewpoints().deserialize(json));
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[0], 0.5);
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[1], 0.05);
  viewpoints().setCaptionPosition(-1.0, 4.0);
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[0], 0.0);
  EXPECT_DOUBLE_EQ(viewpoints().captionPosition()[1], 1.0);

  // A state file with no viewpoints in it is not a list of none.
  EXPECT_FALSE(viewpoints().deserialize(QJsonObject()));
}

// Building a live animation needs a running ParaView application (the
// base class reaches ActiveObjects for the time keeper), so what is
// checked here is the half that does not: an entry is only rebuilt if
// the state file still describes something that can carry it.
TEST_F(AnimationTest, SnapshotsKeepThresholdsAndHiddenLabelsThroughJson)
{
  SinkSnapshot snapshot;
  snapshot.visible = false;
  snapshot.thresholdLower = 12.5;
  snapshot.thresholdUpper = 80.0;
  snapshot.hiddenLabels = QVector<double>{ 2, 5, 9 };

  auto restored = SinkSnapshot::deserialize(snapshot.serialize());
  EXPECT_FALSE(restored.visible);
  ASSERT_TRUE(restored.thresholdLower && restored.thresholdUpper);
  EXPECT_DOUBLE_EQ(*restored.thresholdLower, 12.5);
  EXPECT_DOUBLE_EQ(*restored.thresholdUpper, 80.0);
  ASSERT_TRUE(restored.hiddenLabels);
  EXPECT_EQ(*restored.hiddenLabels, (QVector<double>{ 2, 5, 9 }));

  // Fields a module does not have stay unset
  SinkSnapshot plain;
  auto plainRestored = SinkSnapshot::deserialize(plain.serialize());
  EXPECT_FALSE(plainRestored.thresholdLower);
  EXPECT_FALSE(plainRestored.hiddenLabels);
}

TEST_F(AnimationTest, RecordedThresholdChangesNameTheEndThatMoved)
{
  tomviz::pipeline::Pipeline pipeline;
  auto* threshold = new tomviz::pipeline::ThresholdSink();
  pipeline.addNode(threshold);
  const int id = pipeline.nodeId(threshold);

  auto record = [&](double lower, double upper) {
    Viewpoint viewpoint = viewpointAt(0.0, 1, false);
    SinkSnapshot snapshot;
    snapshot.thresholdLower = lower;
    snapshot.thresholdUpper = upper;
    viewpoint.scene.sinks.insert(id, snapshot);
    viewpoint.scene.recorded = true;
    viewpoints().append(viewpoint);
  };
  record(10.0, 90.0);
  record(40.0, 90.0);
  record(40.0, 60.0);
  record(20.0, 70.0);

  auto rows = RecordedAnimations::instance().changes(&pipeline);
  ASSERT_EQ(rows.size(), 3);
  EXPECT_EQ(rows[0].property, "threshold");
  EXPECT_EQ(rows[0].controlProperty, "thresholdLower");
  EXPECT_DOUBLE_EQ(*rows[0].startValue, 10.0);
  EXPECT_DOUBLE_EQ(*rows[0].stopValue, 40.0);
  EXPECT_EQ(rows[1].controlProperty, "thresholdUpper");
  EXPECT_DOUBLE_EQ(*rows[1].stopValue, 60.0);
  // Both ends moved: nothing a single control can show
  EXPECT_EQ(rows[2].controlProperty, "");
  EXPECT_FALSE(rows[2].startValue);

  // Removing the row gives the later viewpoint the earlier range
  RecordedAnimations::instance().remove(rows[0], &pipeline);
  const auto& later = viewpoints().at(1).scene.sinks[id];
  EXPECT_DOUBLE_EQ(*later.thresholdLower, 10.0);
}

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
  auto first = viewpointAt(0, 1, true);
  first.name = viewpoints().nextDefaultName();
  viewpoints().append(first);
  EXPECT_EQ(first.name, QString("Viewpoint 1"));

  auto second = viewpointAt(1, 1, true);
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

  viewpoints().append(viewpointAt(0, 1, false));
  viewpoints().append(viewpointAt(1, 3, false));
  viewpoints().append(viewpointAt(2, 1, false));

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

TEST_F(AnimationTest, RecordedChangesListWhatDiffersBetweenViewpoints)
{
  tomviz::pipeline::Pipeline pipeline;
  auto* older = new tomviz::pipeline::SliceSink();
  pipeline.addNode(older);
  older->setOpacity(1.0);
  Viewpoint first;
  first.name = "Start";
  first.scene = SceneSnapshot::capture(&pipeline);
  viewpoints().append(first);

  // A blank viewpoint in the middle records nothing and is skipped over
  Viewpoint blank;
  blank.name = "Blank";
  viewpoints().append(blank);

  auto* added = new tomviz::pipeline::SliceSink();
  pipeline.addNode(added);
  added->setOpacity(0.6);
  older->setOpacity(0.4);
  Viewpoint last;
  last.name = "End";
  last.scene = SceneSnapshot::capture(&pipeline);
  viewpoints().append(last);

  auto& recorded = RecordedAnimations::instance();
  auto changes = recorded.changes(&pipeline);
  ASSERT_EQ(changes.size(), 2);
  // Pipeline order: the older slice first
  EXPECT_EQ(changes[0].nodeId, pipeline.nodeId(older));
  EXPECT_EQ(changes[0].property, "opacity");
  EXPECT_EQ(changes[0].description, "opacity 1 to 0.4");
  EXPECT_EQ(changes[0].fromAnchor, 0);
  EXPECT_EQ(changes[0].toAnchor, 2);
  EXPECT_EQ(changes[1].nodeId, pipeline.nodeId(added));
  EXPECT_EQ(changes[1].description, "fades in to 0.6");

  // Both slices are known to the path; Go To the first viewpoint hides
  // the one it never saw, and a blank viewpoint touches nothing
  auto known = recorded.recordedNodeIds();
  EXPECT_TRUE(known.contains(pipeline.nodeId(added)));
  viewpoints().at(0).scene.apply(&pipeline, &known);
  EXPECT_FALSE(added->visibility());
  EXPECT_TRUE(older->visibility());
  added->setVisibility(true);
  viewpoints().at(1).scene.apply(&pipeline, &known);
  EXPECT_TRUE(added->visibility());
  // A module no viewpoint recorded is left alone by Go To
  auto* stranger = new tomviz::pipeline::SliceSink();
  pipeline.addNode(stranger);
  viewpoints().at(0).scene.apply(&pipeline, &known);
  EXPECT_TRUE(stranger->visibility());

  // Removing a row edits the later viewpoint: the older slice keeps its
  // opacity of 1 there, so only the fade-in is left
  recorded.remove(changes[0], &pipeline);
  changes = recorded.changes(&pipeline);
  ASSERT_EQ(changes.size(), 1);
  EXPECT_EQ(changes[0].nodeId, pipeline.nodeId(added));
  const auto& edited = viewpoints().at(2).scene.sinks[pipeline.nodeId(older)];
  EXPECT_DOUBLE_EQ(*edited.opacity, 1.0);

  // Removing the fade-in leaves the added slice hidden at the end too
  recorded.remove(changes[0], &pipeline);
  EXPECT_TRUE(recorded.changes(&pipeline).isEmpty());
  EXPECT_FALSE(viewpoints().at(2).scene.sinks[pipeline.nodeId(added)].visible);
}

TEST_F(AnimationTest, RecordedChangesIncludeSlicePositionsAndIsoValues)
{
  tomviz::pipeline::Pipeline pipeline;
  auto* slice = new tomviz::pipeline::SliceSink();
  auto* contour = new tomviz::pipeline::ContourSink();
  pipeline.addNode(slice);
  pipeline.addNode(contour);
  slice->setSlice(10);
  contour->setIsoValue(100.0);
  Viewpoint first;
  first.name = "A";
  first.scene = SceneSnapshot::capture(&pipeline);
  viewpoints().append(first);

  slice->setSlice(30);
  contour->setIsoValue(250.0);
  Viewpoint second;
  second.name = "B";
  second.scene = SceneSnapshot::capture(&pipeline);
  viewpoints().append(second);

  auto& recorded = RecordedAnimations::instance();
  auto changes = recorded.changes(&pipeline);
  ASSERT_EQ(changes.size(), 2);
  EXPECT_EQ(changes[0].property, "slice");
  EXPECT_EQ(changes[0].description, "slice 10 to 30");
  ASSERT_TRUE(changes[0].startValue && changes[0].stopValue);
  EXPECT_DOUBLE_EQ(*changes[0].startValue, 10.0);
  EXPECT_DOUBLE_EQ(*changes[0].stopValue, 30.0);
  EXPECT_EQ(changes[1].property, "iso");
  EXPECT_EQ(changes[1].description, "iso value 100 to 250");

  // The positions survive a state file round trip and Go To restores
  // them
  auto restored = Viewpoint::deserialize(second.serialize());
  slice->setSlice(0);
  contour->setIsoValue(0.0);
  restored.scene.apply(&pipeline);
  EXPECT_EQ(slice->slice(), 30);
  EXPECT_DOUBLE_EQ(contour->isoValue(), 250.0);

  // Removing the slice row pins B's slice to A's
  recorded.remove(changes[0], &pipeline);
  changes = recorded.changes(&pipeline);
  ASSERT_EQ(changes.size(), 1);
  EXPECT_EQ(changes[0].property, "iso");
  EXPECT_EQ(*viewpoints().at(1).scene.sinks[pipeline.nodeId(slice)].sliceIndex,
            10);
}

TEST_F(AnimationTest, AnchorSpansFollowThePathStops)
{
  viewpoints().append(viewpointAt(0, 1, false));
  viewpoints().append(viewpointAt(1, 1, false));
  viewpoints().append(viewpointAt(2, 2, false));
  // Stops at 0, 0.25, 1; anchors 0 and 2 recorded
  QList<int> anchors = { 0, 2 };
  auto before = tomviz::anchorSpanAt(anchors, -0.1);
  EXPECT_EQ(before.from, 0);
  EXPECT_EQ(before.to, 0);
  auto middle = tomviz::anchorSpanAt(anchors, 0.5);
  EXPECT_EQ(middle.from, 0);
  EXPECT_EQ(middle.to, 2);
  EXPECT_DOUBLE_EQ(middle.u, 0.5);
  auto after = tomviz::anchorSpanAt(anchors, 1.0);
  EXPECT_EQ(after.from, 2);
  EXPECT_EQ(after.to, 2);
  EXPECT_DOUBLE_EQ(after.u, 1.0);
}

