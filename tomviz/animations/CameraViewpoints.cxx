/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CameraViewpoints.h"

#include "ActiveObjects.h"
#include "CameraAnimation.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <vtkCamera.h>
#include <vtkCameraInterpolator.h>
#include <vtkMath.h>
#include <vtkTupleInterpolator.h>

#include <algorithm>
#include <cmath>

namespace tomviz {

namespace {

QJsonArray toArray(const std::array<double, 3>& v)
{
  return QJsonArray({ v[0], v[1], v[2] });
}

std::array<double, 3> toVector(const QJsonValue& value,
                               const std::array<double, 3>& fallback)
{
  auto array = value.toArray();
  if (array.size() != 3) {
    return fallback;
  }
  return { array[0].toDouble(), array[1].toDouble(), array[2].toDouble() };
}

} // anonymous namespace

void Viewpoint::readFrom(vtkCamera* camera)
{
  camera->GetPosition(position.data());
  camera->GetFocalPoint(focalPoint.data());
  camera->GetViewUp(viewUp.data());
  viewAngle = camera->GetViewAngle();
  parallelScale = camera->GetParallelScale();
  parallelProjection = camera->GetParallelProjection() != 0;
}

void Viewpoint::applyTo(vtkCamera* camera) const
{
  camera->SetPosition(position[0], position[1], position[2]);
  camera->SetFocalPoint(focalPoint[0], focalPoint[1], focalPoint[2]);
  camera->SetViewUp(viewUp[0], viewUp[1], viewUp[2]);
  camera->SetViewAngle(viewAngle);
  camera->SetParallelScale(parallelScale);
  camera->SetParallelProjection(parallelProjection ? 1 : 0);
}

QJsonObject Viewpoint::serialize() const
{
  QJsonObject json;
  json["position"] = toArray(position);
  json["focalPoint"] = toArray(focalPoint);
  json["viewUp"] = toArray(viewUp);
  json["viewAngle"] = viewAngle;
  json["parallelScale"] = parallelScale;
  json["parallelProjection"] = parallelProjection;
  json["legFrames"] = legFrames;
  json["eased"] = eased;
  if (orbitTurns != 0) {
    json["orbitTurns"] = orbitTurns;
    json["orbitFrames"] = orbitFrames;
  }
  json["orbitEased"] = orbitEased;
  json["name"] = name;
  if (!label.isEmpty()) {
    json["label"] = label;
  }
  if (!thumbnail.isEmpty()) {
    json["thumbnail"] = QString::fromLatin1(thumbnail.toBase64());
  }
  if (!scene.isEmpty()) {
    json["scene"] = scene.serialize();
  }
  return json;
}

Viewpoint Viewpoint::deserialize(const QJsonObject& json)
{
  Viewpoint viewpoint;
  viewpoint.position = toVector(json["position"], viewpoint.position);
  viewpoint.focalPoint = toVector(json["focalPoint"], viewpoint.focalPoint);
  viewpoint.viewUp = toVector(json["viewUp"], viewpoint.viewUp);
  viewpoint.viewAngle = json["viewAngle"].toDouble(viewpoint.viewAngle);
  viewpoint.parallelScale =
    json["parallelScale"].toDouble(viewpoint.parallelScale);
  viewpoint.parallelProjection =
    json["parallelProjection"].toBool(viewpoint.parallelProjection);
  viewpoint.legFrames = json["legFrames"].toInt(viewpoint.legFrames);
  viewpoint.eased = json["eased"].toBool(viewpoint.eased);
  viewpoint.orbitTurns = json["orbitTurns"].toInt(0);
  viewpoint.orbitFrames = json["orbitFrames"].toInt(viewpoint.orbitFrames);
  // Before orbits had their own easing they followed the leg's, so an
  // orbit saved then plays as it did; a viewpoint saved then without
  // one gets the default
  viewpoint.orbitEased =
    json.contains("orbitEased")
      ? json["orbitEased"].toBool()
      : (viewpoint.orbitTurns != 0 ? viewpoint.eased : false);
  viewpoint.name = json["name"].toString();
  viewpoint.label = json["label"].toString();
  viewpoint.thumbnail =
    QByteArray::fromBase64(json["thumbnail"].toString().toLatin1());
  viewpoint.scene = SceneSnapshot::deserialize(json["scene"].toObject());
  return viewpoint;
}

void orbitAt(const Viewpoint& viewpoint, double u, vtkCamera* camera)
{
  if (!camera) {
    return;
  }
  u = std::clamp(u, 0.0, 1.0);
  viewpoint.applyTo(camera);

  // The offset from the focal point splits into a height along the
  // view-up axis, which the orbit keeps, and a radius in the plane
  // across it, which it swings round.
  double axis[3] = { viewpoint.viewUp[0], viewpoint.viewUp[1],
                     viewpoint.viewUp[2] };
  if (vtkMath::Normalize(axis) == 0.0) {
    return;
  }
  double offset[3];
  for (int k = 0; k < 3; ++k) {
    offset[k] = viewpoint.position[k] - viewpoint.focalPoint[k];
  }
  const double height = vtkMath::Dot(offset, axis);
  double e1[3];
  for (int k = 0; k < 3; ++k) {
    e1[k] = offset[k] - height * axis[k];
  }
  const double radius = vtkMath::Normalize(e1);
  if (radius < 1e-9) {
    // Looking straight along the axis: nothing to swing.
    return;
  }
  double e2[3];
  vtkMath::Cross(axis, e1, e2);

  const double angle = viewpoint.orbitTurns * 2.0 * vtkMath::Pi() * u;
  double position[3];
  for (int k = 0; k < 3; ++k) {
    position[k] = viewpoint.focalPoint[k] +
                  radius * (std::cos(angle) * e1[k] + std::sin(angle) * e2[k]) +
                  height * axis[k];
  }
  // Both ends are the viewpoint itself; say so exactly rather than to
  // rounding.
  if (u <= 0.0 || u >= 1.0) {
    for (int k = 0; k < 3; ++k) {
      position[k] = viewpoint.position[k];
    }
  }
  camera->SetPosition(position);
}

void CameraViewpoints::noteChanged()
{
  m_interpolatorStale = true;
  // No path, no flight: whoever emptied the list (Reset, a state file,
  // the last Remove) need not know about the flight for it to stop.
  if (!isPath()) {
    stopFlight();
  }
  emit changed();
}

CameraViewpoints& CameraViewpoints::instance()
{
  static CameraViewpoints viewpoints;
  return viewpoints;
}

void CameraViewpoints::append(const Viewpoint& viewpoint)
{
  m_viewpoints.append(viewpoint);
  noteChanged();
}

void CameraViewpoints::replace(int index, const Viewpoint& viewpoint)
{
  if (index < 0 || index >= m_viewpoints.size()) {
    return;
  }

  m_viewpoints[index] = viewpoint;
  noteChanged();
}

void CameraViewpoints::removeAt(int index)
{
  if (index < 0 || index >= m_viewpoints.size()) {
    return;
  }

  m_viewpoints.removeAt(index);
  noteChanged();
}

void CameraViewpoints::move(int from, int to)
{
  if (from < 0 || from >= m_viewpoints.size() || to < 0 ||
      to >= m_viewpoints.size() || from == to) {
    return;
  }

  m_viewpoints.move(from, to);
  noteChanged();
}

void CameraViewpoints::clear()
{
  if (m_viewpoints.isEmpty()) {
    return;
  }

  m_viewpoints.clear();
  noteChanged();
}

bool CameraViewpoints::isPath() const
{
  return m_viewpoints.size() >= 2 ||
         (m_viewpoints.size() == 1 && m_viewpoints[0].orbitTurns != 0);
}

int CameraViewpoints::totalFrames() const
{
  if (!isPath()) {
    return 0;
  }
  int total = 0;
  for (int i = 0; i < m_viewpoints.size(); ++i) {
    if (m_viewpoints[i].orbitTurns != 0) {
      total += std::max(0, m_viewpoints[i].orbitFrames);
    }
    if (i + 1 < m_viewpoints.size()) {
      total += std::max(0, m_viewpoints[i].legFrames);
    }
  }
  // A start and an end at the least: legs of no length are cuts, and
  // one frame is not an animation the player can run.
  return std::max(2, total);
}

namespace {

struct Timeline
{
  QList<double> arrivals;
  QList<double> departures;
};

// Legs and orbits laid end to end, each weighted by its duration: the
// orbit at a viewpoint runs from its arrival to its departure, the leg
// to the next viewpoint from that departure to the next arrival.
Timeline timelineOf(const QList<Viewpoint>& viewpoints)
{
  Timeline timeline;
  const int count = viewpoints.size();
  QList<double> dwells;
  QList<double> legs;
  double total = 0;
  for (int i = 0; i < count; ++i) {
    const double dwell = viewpoints[i].orbitTurns != 0
                           ? std::max(0, viewpoints[i].orbitFrames)
                           : 0.0;
    dwells.append(dwell);
    total += dwell;
    if (i + 1 < count) {
      const double leg = std::max(0, viewpoints[i].legFrames);
      legs.append(leg);
      total += leg;
    }
  }

  // Everything was given a zero (or negative) duration, which would
  // leave no time to run the path in. Spread it evenly instead.
  if (total <= 0) {
    legs.fill(1.0);
    for (int i = 0; i < count; ++i) {
      if (viewpoints[i].orbitTurns != 0) {
        dwells[i] = 1.0;
      }
    }
    total = legs.size() + std::count_if(dwells.begin(), dwells.end(),
                                        [](double d) { return d > 0; });
  }

  double elapsed = 0;
  for (int i = 0; i < count; ++i) {
    timeline.arrivals.append(elapsed / total);
    elapsed += dwells[i];
    timeline.departures.append(elapsed / total);
    if (i + 1 < count) {
      elapsed += legs[i];
    }
  }
  // Guard the ends against accumulated rounding: callers treat them as
  // the start and the end of the path.
  timeline.arrivals.first() = 0.0;
  timeline.departures.last() = 1.0;
  if (count > 1) {
    timeline.arrivals.last() =
      std::min(timeline.arrivals.last(), timeline.departures.last());
  }
  return timeline;
}

} // namespace

QList<double> CameraViewpoints::stops() const
{
  if (!isPath()) {
    return {};
  }
  return timelineOf(m_viewpoints).arrivals;
}

QList<double> CameraViewpoints::departures() const
{
  if (!isPath()) {
    return {};
  }
  return timelineOf(m_viewpoints).departures;
}

QString CameraViewpoints::nextDefaultName() const
{
  int highest = 0;
  QRegularExpression pattern("^Viewpoint (\\d+)$");
  for (const auto& viewpoint : m_viewpoints) {
    auto match = pattern.match(viewpoint.name);
    if (match.hasMatch()) {
      highest = std::max(highest, match.captured(1).toInt());
    }
  }
  return QString("Viewpoint %1").arg(highest + 1);
}

double CameraViewpoints::anchorTime(int anchor) const
{
  auto stopList = stops();
  if (stopList.isEmpty()) {
    return anchor <= 0 ? 0.0 : 1.0;
  }
  return stopList[qBound(0, anchor, static_cast<int>(stopList.size()) - 1)];
}

double CameraViewpoints::departureTime(int anchor) const
{
  auto departureList = departures();
  if (departureList.isEmpty()) {
    return anchor <= 0 ? 0.0 : 1.0;
  }
  return departureList[qBound(0, anchor,
                              static_cast<int>(departureList.size()) - 1)];
}

double CameraViewpoints::remapProgress(double progress) const
{
  if (!isPath()) {
    return 0.0;
  }
  const auto timeline = timelineOf(m_viewpoints);
  const double p = qBound(0.0, progress, 1.0);

  // Walk the pieces in order: the orbit at each viewpoint, then the leg
  // leaving it. The last piece owns p == 1. A piece of no length is
  // skipped over.
  const int count = m_viewpoints.size();
  for (int i = 0; i < count; ++i) {
    const double pieces[2][2] = {
      { timeline.arrivals[i], timeline.departures[i] },
      { timeline.departures[i],
        i + 1 < count ? timeline.arrivals[i + 1] : timeline.departures[i] }
    };
    for (int k = 0; k < 2; ++k) {
      const double start = pieces[k][0];
      const double stop = pieces[k][1];
      const double span = stop - start;
      if (span <= 0) {
        continue;
      }
      const bool last = stop >= 1.0;
      if (p < stop || last) {
        double u = (p - start) / span;
        const bool eased = k == 0 ? m_viewpoints[i].orbitEased
                                  : m_viewpoints[i].eased;
        if (eased) {
          // Smoothstep: zero slope at both ends, so the camera
          // accelerates away from a stop and decelerates into the next.
          u = u * u * (3.0 - 2.0 * u);
        }
        return start + u * span;
      }
    }
  }
  return 1.0;
}

double CameraViewpoints::segmentProgress(double progress, int segment) const
{
  auto stopList = stops();
  if (segment < 0 || segment + 1 >= stopList.size()) {
    return progress;
  }

  // The leg runs from leaving one viewpoint to reaching the next; an
  // orbit at either end is time the animation holds still.
  const double start = departures()[segment];
  const double stop = stopList[segment + 1];
  const double span = stop - start;
  if (span <= 0) {
    return progress >= stop ? 1.0 : 0.0;
  }

  return qBound(0.0, (progress - start) / span, 1.0);
}

void CameraViewpoints::interpolate(double t, vtkCamera* camera)
{
  if (!camera || !isPath()) {
    return;
  }

  if (m_interpolatorStale) {
    rebuildInterpolator();
  }

  const auto timeline = timelineOf(m_viewpoints);
  t = qBound(0.0, t, 1.0);

  // The viewpoint whose orbit or leg t falls in: the last one reached.
  const int count = m_viewpoints.size();
  int current = 0;
  while (current + 1 < count && t >= timeline.arrivals[current + 1]) {
    ++current;
  }

  const auto& viewpoint = m_viewpoints[current];
  const double departure = timeline.departures[current];
  if (viewpoint.orbitTurns != 0 &&
      (t < departure || current + 1 == count)) {
    const double span = departure - timeline.arrivals[current];
    const double u =
      span > 0 ? (t - timeline.arrivals[current]) / span : 1.0;
    orbitAt(viewpoint, u, camera);
    return;
  }

  for (const auto& run : m_runs) {
    if (current >= run.first && current < run.last) {
      run.interpolator->InterpolateCamera(t, camera);
      return;
    }
  }
  // A viewpoint in no run (between two orbits, or the last) is held.
  viewpoint.applyTo(camera);
}

void CameraViewpoints::rebuildInterpolator()
{
  m_runs.clear();
  m_interpolatorStale = false;
  if (!isPath()) {
    return;
  }
  const auto timeline = timelineOf(m_viewpoints);

  // A spline needs three points to curve through; with two it would
  // ease in and out of the straight line on its own, on top of the
  // segment's easing, so two viewpoints get a plain linear blend.
  //
  // The interpolators are supplied ready-made rather than through
  // vtkCameraInterpolator's Linear/Spline modes: in the VTK we build
  // against, changing a vtkTupleInterpolator's type wipes its component
  // count, and the camera interpolator sets the count first and the type
  // second, so its linear mode stores nothing and hands back
  // uninitialized memory, which parked the camera at the origin on every
  // two-viewpoint path. In manual mode it leaves the types alone.
  //
  // A run's first camera is added at its departure (the orbit there, if
  // any, is over) and the others at their arrival. A leg of no length
  // puts two cameras at the same time, where the later one replaces the
  // earlier (the path has arrived), so it is the distinct times that
  // decide whether there is anything for a spline to curve through.
  auto makeRun = [this, &timeline](int first, int last) {
    Run run;
    run.first = first;
    run.last = last;
    int distinct = 0;
    double previous = -1.0;
    for (int i = first; i <= last; ++i) {
      const double t =
        i == first ? timeline.departures[i] : timeline.arrivals[i];
      if (t != previous) {
        ++distinct;
      }
      previous = t;
    }
    const bool linear = distinct < 3;
    auto tuple = [linear]() {
      auto interpolator = vtkSmartPointer<vtkTupleInterpolator>::New();
      if (linear) {
        interpolator->SetInterpolationTypeToLinear();
      } else {
        interpolator->SetInterpolationTypeToSpline();
      }
      return interpolator;
    };
    run.interpolator = vtkSmartPointer<vtkCameraInterpolator>::New();
    run.interpolator->SetInterpolationTypeToManual();
    run.interpolator->SetPositionInterpolator(tuple());
    run.interpolator->SetFocalPointInterpolator(tuple());
    run.interpolator->SetViewUpInterpolator(tuple());
    run.interpolator->SetViewAngleInterpolator(tuple());
    run.interpolator->SetParallelScaleInterpolator(tuple());
    run.interpolator->SetClippingRangeInterpolator(tuple());
    for (int i = first; i <= last; ++i) {
      vtkNew<vtkCamera> camera;
      m_viewpoints[i].applyTo(camera);
      run.interpolator->AddCamera(
        i == first ? timeline.departures[i] : timeline.arrivals[i], camera);
    }
    return run;
  };

  // A spline cannot hold still, so a viewpoint that orbits ends the run
  // arriving at it and starts the next one leaving it.
  int first = 0;
  for (int i = 0; i < m_viewpoints.size(); ++i) {
    const bool last = i + 1 == m_viewpoints.size();
    if (last || m_viewpoints[i].orbitTurns != 0) {
      if (i > first) {
        m_runs.push_back(makeRun(first, i));
      }
      first = i;
    }
  }
}

void CameraViewpoints::startFlight(pqRenderView* view, bool snapToHead)
{
  stopFlight();
  if (view && isPath()) {
    auto* flight = new CameraAnimation(view);
    m_flight = flight;
    // Playback that starts at time zero never announces a time change,
    // so put the camera at the head of the path now rather than leaving
    // it wherever it was until the clock first moves. Callers that arm
    // the flight as a side effect (e.g. adding a viewpoint) skip the
    // snap so the camera does not jump away from where the user just
    // framed it.
    if (snapToHead) {
      flight->onTimeChanged();
    }
  }
}

void CameraViewpoints::stopFlight()
{
  if (m_flight) {
    m_flight->deleteLater();
    m_flight = nullptr;
  }
}

bool CameraViewpoints::isFlying() const
{
  return !m_flight.isNull();
}

bool CameraViewpoints::syncFlight(pqRenderView* view)
{
  if (!isPath()) {
    stopFlight();
    return false;
  }
  if (isFlying() || !view) {
    return false;
  }
  startFlight(view, /*snapToHead=*/false);
  return isFlying();
}

void CameraViewpoints::setCaptionPosition(double x, double y)
{
  x = std::clamp(x, 0.0, 1.0);
  y = std::clamp(y, 0.0, 1.0);
  if (x == m_captionPosition[0] && y == m_captionPosition[1]) {
    return;
  }
  m_captionPosition = { x, y };
  emit captionPositionChanged();
}

QJsonObject CameraViewpoints::serialize() const
{
  QJsonArray array;
  for (const auto& viewpoint : m_viewpoints) {
    array.append(viewpoint.serialize());
  }

  QJsonObject json;
  json["viewpoints"] = array;
  json["flying"] = isFlying();
  json["captionPosition"] =
    QJsonArray{ m_captionPosition[0], m_captionPosition[1] };
  return json;
}

bool CameraViewpoints::deserialize(const QJsonObject& json)
{
  if (!json["viewpoints"].isArray()) {
    return false;
  }

  m_viewpoints.clear();
  const auto entries = json["viewpoints"].toArray();
  for (const auto& value : entries) {
    m_viewpoints.append(Viewpoint::deserialize(value.toObject()));
    // Files from before viewpoints had names get positional ones.
    if (m_viewpoints.last().name.isEmpty()) {
      m_viewpoints.last().name =
        QString("Viewpoint %1").arg(m_viewpoints.size());
    }
  }

  // Files from before legs and orbits had frame counts of their own
  // carried relative durations and one total: each piece took its share
  // of the total, so that share is its frame count now.
  bool legacy = !entries.isEmpty();
  for (const auto& value : entries) {
    if (value.toObject().contains("legFrames")) {
      legacy = false;
    }
  }
  if (legacy) {
    const int count = m_viewpoints.size();
    const int total = std::max(2, json["numberOfFrames"].toInt(200));
    QList<double> legWeights;
    QList<double> orbitWeights;
    double sum = 0;
    for (int i = 0; i < count; ++i) {
      const auto entry = entries[i].toObject();
      const double leg = i + 1 < count
                           ? std::max(0.0, entry["duration"].toDouble(1.0))
                           : 0.0;
      const double orbit =
        m_viewpoints[i].orbitTurns != 0
          ? std::max(0.0, entry["orbitDuration"].toDouble(1.0))
          : 0.0;
      legWeights.append(leg);
      orbitWeights.append(orbit);
      sum += leg + orbit;
    }
    for (int i = 0; i < count; ++i) {
      auto share = [&](double weight) {
        return sum > 0 ? std::max(1, static_cast<int>(std::lround(
                                       total * weight / sum)))
                       : total;
      };
      if (i + 1 < count) {
        m_viewpoints[i].legFrames = share(legWeights[i]);
      }
      if (m_viewpoints[i].orbitTurns != 0) {
        m_viewpoints[i].orbitFrames = share(orbitWeights[i]);
      }
    }
  }

  // Captions are centered on the position now. The old default put their
  // corner just inside the view's, where a centered caption would be cut
  // in half, so it becomes the new default, as does a file from before
  // the caption could be moved.
  auto position = json["captionPosition"].toArray();
  const double x = position.size() == 2 ? position[0].toDouble(0.5) : 0.5;
  const double y = position.size() == 2 ? position[1].toDouble(0.05) : 0.05;
  if (x == 0.02 && y == 0.03) {
    setCaptionPosition(0.5, 0.05);
  } else {
    setCaptionPosition(x, y);
  }

  noteChanged();
  return true;
}

} // namespace tomviz
