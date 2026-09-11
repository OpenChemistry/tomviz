/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CameraViewpoints.h"

#include "ActiveObjects.h"
#include "CameraAnimation.h"
#include "SceneAnimation.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <vtkCamera.h>
#include <vtkCameraInterpolator.h>

#include <algorithm>

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
  json["duration"] = duration;
  json["eased"] = eased;
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
  viewpoint.duration = json["duration"].toDouble(viewpoint.duration);
  viewpoint.eased = json["eased"].toBool(viewpoint.eased);
  viewpoint.name = json["name"].toString();
  viewpoint.label = json["label"].toString();
  viewpoint.thumbnail =
    QByteArray::fromBase64(json["thumbnail"].toString().toLatin1());
  viewpoint.scene = SceneSnapshot::deserialize(json["scene"].toObject());
  return viewpoint;
}

CameraViewpoints& CameraViewpoints::instance()
{
  static CameraViewpoints viewpoints;
  return viewpoints;
}

void CameraViewpoints::append(const Viewpoint& viewpoint)
{
  m_viewpoints.append(viewpoint);
  m_interpolatorStale = true;
  emit changed();
}

void CameraViewpoints::replace(int index, const Viewpoint& viewpoint)
{
  if (index < 0 || index >= m_viewpoints.size()) {
    return;
  }

  m_viewpoints[index] = viewpoint;
  m_interpolatorStale = true;
  emit changed();
}

void CameraViewpoints::removeAt(int index)
{
  if (index < 0 || index >= m_viewpoints.size()) {
    return;
  }

  m_viewpoints.removeAt(index);
  m_interpolatorStale = true;
  emit changed();
}

void CameraViewpoints::move(int from, int to)
{
  if (from < 0 || from >= m_viewpoints.size() || to < 0 ||
      to >= m_viewpoints.size() || from == to) {
    return;
  }

  m_viewpoints.move(from, to);
  m_interpolatorStale = true;
  emit changed();
}

void CameraViewpoints::clear()
{
  if (m_viewpoints.isEmpty()) {
    return;
  }

  m_viewpoints.clear();
  m_interpolatorStale = true;
  emit changed();
}

QList<double> CameraViewpoints::stops() const
{
  int count = m_viewpoints.size();
  if (count < 2) {
    return {};
  }

  QList<double> durations;
  double total = 0;
  for (int i = 0; i < count - 1; ++i) {
    double duration = std::max(0.0, m_viewpoints[i].duration);
    durations.append(duration);
    total += duration;
  }

  // Every segment was given a zero (or negative) duration, which would
  // leave no time to run the path in. Spread it evenly instead.
  if (total <= 0) {
    durations.fill(1.0);
    total = count - 1;
  }

  QList<double> stops;
  stops.reserve(count);
  stops.append(0.0);
  double elapsed = 0;
  for (int i = 0; i < count - 1; ++i) {
    elapsed += durations[i];
    stops.append(elapsed / total);
  }
  // Guard the last stop against accumulated rounding: the caller treats
  // it as the end of the path.
  stops.last() = 1.0;

  return stops;
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
  if (stopList.size() < 2) {
    return anchor <= 0 ? 0.0 : 1.0;
  }
  return stopList[qBound(0, anchor, static_cast<int>(stopList.size()) - 1)];
}

double CameraViewpoints::remapProgress(double progress) const
{
  auto stopList = stops();
  if (stopList.size() < 2) {
    return 0.0;
  }

  double p = qBound(0.0, progress, 1.0);

  // The last segment owns p == 1, so stop one short of the end.
  int segment = 0;
  while (segment + 2 < stopList.size() && p >= stopList[segment + 1]) {
    ++segment;
  }

  double span = stopList[segment + 1] - stopList[segment];
  if (span <= 0) {
    return stopList[segment + 1];
  }

  double u = (p - stopList[segment]) / span;
  if (m_viewpoints[segment].eased) {
    // Smoothstep: zero slope at both ends, so the camera accelerates
    // away from one viewpoint and decelerates into the next.
    u = u * u * (3.0 - 2.0 * u);
  }

  return stopList[segment] + u * span;
}

double CameraViewpoints::segmentProgress(double progress, int segment) const
{
  auto stopList = stops();
  if (segment < 0 || segment + 1 >= stopList.size()) {
    return progress;
  }

  double span = stopList[segment + 1] - stopList[segment];
  if (span <= 0) {
    return progress >= stopList[segment] ? 1.0 : 0.0;
  }

  return qBound(0.0, (progress - stopList[segment]) / span, 1.0);
}

void CameraViewpoints::interpolate(double t, vtkCamera* camera)
{
  if (!camera || m_viewpoints.size() < 2) {
    return;
  }

  if (!m_interpolator) {
    m_interpolator = vtkSmartPointer<vtkCameraInterpolator>::New();
    m_interpolatorStale = true;
  }

  if (m_interpolatorStale) {
    rebuildInterpolator();
  }

  m_interpolator->InterpolateCamera(qBound(0.0, t, 1.0), camera);
}

void CameraViewpoints::rebuildInterpolator()
{
  m_interpolator->Initialize();
  m_interpolatorStale = false;

  auto stopList = stops();
  if (stopList.size() != m_viewpoints.size()) {
    return;
  }

  // A spline needs three points to curve through; with two it is just a
  // slower way of drawing a straight line.
  if (m_viewpoints.size() < 3) {
    m_interpolator->SetInterpolationTypeToLinear();
  } else {
    m_interpolator->SetInterpolationTypeToSpline();
  }

  for (int i = 0; i < m_viewpoints.size(); ++i) {
    vtkNew<vtkCamera> camera;
    m_viewpoints[i].applyTo(camera);
    m_interpolator->AddCamera(stopList[i], camera);
  }
}

void CameraViewpoints::startFlight(pqRenderView* view, bool snapToHead)
{
  stopFlight();
  if (view && m_viewpoints.size() >= 2) {
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
    // The recorded module state travels with the camera
    auto* sceneFlight =
      new SceneAnimation(ActiveObjects::instance().pipeline());
    m_sceneFlight = sceneFlight;
    if (snapToHead) {
      sceneFlight->onTimeChanged();
    }
  }
}

void CameraViewpoints::stopFlight()
{
  if (m_flight) {
    m_flight->deleteLater();
    m_flight = nullptr;
  }
  if (m_sceneFlight) {
    m_sceneFlight->deleteLater();
    m_sceneFlight = nullptr;
  }
}

bool CameraViewpoints::isFlying() const
{
  return !m_flight.isNull();
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
  return json;
}

bool CameraViewpoints::deserialize(const QJsonObject& json)
{
  if (!json["viewpoints"].isArray()) {
    return false;
  }

  m_viewpoints.clear();
  for (const auto& value : json["viewpoints"].toArray()) {
    m_viewpoints.append(Viewpoint::deserialize(value.toObject()));
    // Files from before viewpoints had names get positional ones.
    if (m_viewpoints.last().name.isEmpty()) {
      m_viewpoints.last().name =
        QString("Viewpoint %1").arg(m_viewpoints.size());
    }
  }

  m_interpolatorStale = true;
  emit changed();
  return true;
}

} // namespace tomviz
