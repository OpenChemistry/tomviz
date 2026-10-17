/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CameraPath.h"

#include "CameraViewpoints.h"

#include <vtkCamera.h>
#include <vtkMath.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace tomviz {

namespace {

enum Channel
{
  FocalX,
  FocalY,
  FocalZ,
  Distance,
  Azimuth,
  Elevation,
  Roll,
  ViewAngle,
  ParallelScale
};

// @a angle plus whole turns, as near @a reference as it gets: the way
// round that turns the least.
double nearestTurn(double angle, double reference)
{
  const double turn = 2.0 * vtkMath::Pi();
  return angle - turn * std::round((angle - reference) / turn);
}

// Hermite slopes for a curve through (times, values) that never
// overshoots them (Fritsch and Carlson's monotone cubic). A peak, a
// valley or a stop gets slope 0, so between two equal values the curve
// is flat. Times must increase.
std::vector<double> monotoneSlopes(const std::vector<double>& times,
                                   const std::vector<double>& values)
{
  const size_t n = times.size();
  std::vector<double> slopes(n, 0.0);
  if (n < 2) {
    return slopes;
  }
  std::vector<double> h(n - 1), delta(n - 1);
  for (size_t i = 0; i + 1 < n; ++i) {
    h[i] = times[i + 1] - times[i];
    delta[i] = (values[i + 1] - values[i]) / h[i];
  }
  if (n == 2) {
    slopes[0] = slopes[1] = delta[0];
    return slopes;
  }
  for (size_t i = 1; i + 1 < n; ++i) {
    if (delta[i - 1] * delta[i] > 0) {
      const double w1 = 2 * h[i] + h[i - 1];
      const double w2 = h[i] + 2 * h[i - 1];
      slopes[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]);
    }
  }
  // Three-point estimates at the ends, kept from overshooting
  auto end = [](double h0, double h1, double d0, double d1) {
    const double slope = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (slope * d0 <= 0) {
      return 0.0;
    }
    if (d0 * d1 <= 0 && std::abs(slope) > 3 * std::abs(d0)) {
      return 3 * d0;
    }
    return slope;
  };
  slopes[0] = end(h[0], h[1], delta[0], delta[1]);
  slopes[n - 1] = end(h[n - 2], h[n - 3], delta[n - 2], delta[n - 3]);
  return slopes;
}

} // anonymous namespace

void CameraPath::frame(double azimuth, double elevation, double direction[3],
                       double level[3]) const
{
  const double ce = std::cos(elevation);
  const double se = std::sin(elevation);
  for (int k = 0; k < 3; ++k) {
    const double across =
      std::cos(azimuth) * m_e1[k] + std::sin(azimuth) * m_e2[k];
    direction[k] = ce * across + se * m_axis[k];
    level[k] = -se * across + ce * m_axis[k];
  }
}

CameraPath::CameraPath(const QList<Viewpoint>& viewpoints, int first,
                       int last, const std::vector<double>& times)
{
  // Of viewpoints sharing a time (a cut), only the last counts
  std::vector<const Viewpoint*> nodes;
  for (int i = first; i <= last; ++i) {
    const double t = times[i - first];
    if (!m_times.empty() && t <= m_times.back()) {
      nodes.back() = &viewpoints[i];
      continue;
    }
    m_times.push_back(t);
    nodes.push_back(&viewpoints[i]);
  }
  if (nodes.empty()) {
    return;
  }

  // The turntable's axis is the view-up most viewpoints share. A top view
  // among side views has its view-up lying flat, so it is outvoted rather
  // than tipping the turntable over.
  int votes = 0;
  for (auto* candidate : nodes) {
    double up[3] = { candidate->viewUp[0], candidate->viewUp[1],
                     candidate->viewUp[2] };
    if (vtkMath::Normalize(up) == 0) {
      continue;
    }
    int agree = 0;
    for (auto* node : nodes) {
      double other[3] = { node->viewUp[0], node->viewUp[1], node->viewUp[2] };
      if (vtkMath::Normalize(other) > 0 && vtkMath::Dot(up, other) > 0.999) {
        ++agree;
      }
    }
    if (agree > votes) {
      votes = agree;
      std::copy(up, up + 3, m_axis.begin());
    }
  }
  vtkMath::Perpendiculars(m_axis.data(), m_e1.data(), m_e2.data(), 0);

  const size_t n = nodes.size();
  std::vector<double> distance(n), azimuth(n), elevation(n);
  bool measured = false;
  for (size_t i = 0; i < n; ++i) {
    double direction[3];
    for (int k = 0; k < 3; ++k) {
      direction[k] = nodes[i]->position[k] - nodes[i]->focalPoint[k];
    }
    distance[i] = vtkMath::Normalize(direction);
    const double height = vtkMath::Dot(direction, m_axis.data());
    elevation[i] = std::asin(std::clamp(height, -1.0, 1.0));
    double across[3];
    for (int k = 0; k < 3; ++k) {
      across[k] = direction[k] - height * m_axis[k];
    }
    if (vtkMath::Normalize(across) < 1e-6) {
      azimuth[i] = std::numeric_limits<double>::quiet_NaN();
      continue;
    }
    // Azimuth is measured from the first camera off the axis
    if (!measured) {
      std::copy(across, across + 3, m_e1.begin());
      vtkMath::Cross(m_axis.data(), m_e1.data(), m_e2.data());
      measured = true;
    }
    azimuth[i] = std::atan2(vtkMath::Dot(across, m_e2.data()),
                            vtkMath::Dot(across, m_e1.data()));
  }
  // A camera looking straight along the axis takes the azimuth it is
  // flown in from (or out to), so it is reached by tilting over, and its
  // roll makes up the difference.
  for (size_t i = 1; i < n; ++i) {
    if (std::isnan(azimuth[i])) {
      azimuth[i] = azimuth[i - 1];
    }
  }
  for (size_t i = n - 1; i-- > 0;) {
    if (std::isnan(azimuth[i])) {
      azimuth[i] = azimuth[i + 1];
    }
  }

  m_values.resize(n);
  for (size_t i = 0; i < n; ++i) {
    auto& v = m_values[i];
    for (int k = 0; k < 3; ++k) {
      v[FocalX + k] = nodes[i]->focalPoint[k];
    }
    v[Distance] = distance[i];
    v[Azimuth] = std::isnan(azimuth[i]) ? 0.0 : azimuth[i];
    if (i > 0) {
      v[Azimuth] = nearestTurn(v[Azimuth], m_values[i - 1][Azimuth]);
    }
    v[Elevation] = elevation[i];
    v[ViewAngle] = nodes[i]->viewAngle;
    v[ParallelScale] = nodes[i]->parallelScale;

    // Roll: how far the view-up is turned about the line of sight from
    // where a turntable camera here has it
    double direction[3], level[3], cross[3];
    frame(v[Azimuth], v[Elevation], direction, level);
    double up[3] = { nodes[i]->viewUp[0], nodes[i]->viewUp[1],
                     nodes[i]->viewUp[2] };
    const double along = vtkMath::Dot(up, direction);
    for (int k = 0; k < 3; ++k) {
      up[k] -= along * direction[k];
    }
    double roll = 0;
    if (vtkMath::Normalize(up) > 1e-9) {
      vtkMath::Cross(level, up, cross);
      roll =
        std::atan2(vtkMath::Dot(cross, direction), vtkMath::Dot(level, up));
    }
    v[Roll] = i == 0 ? roll : nearestTurn(roll, m_values[i - 1][Roll]);
  }

  m_slopes.assign(n, Values{});
  std::vector<double> channel(n);
  for (int c = 0; c < Channels; ++c) {
    for (size_t i = 0; i < n; ++i) {
      channel[i] = m_values[i][c];
    }
    auto slopes = monotoneSlopes(m_times, channel);
    for (size_t i = 0; i < n; ++i) {
      m_slopes[i][c] = slopes[i];
    }
  }
}

void CameraPath::apply(double t, vtkCamera* camera) const
{
  if (!camera || m_times.empty()) {
    return;
  }

  Values v = t <= m_times.front() ? m_values.front() : m_values.back();
  if (t > m_times.front() && t < m_times.back()) {
    const size_t i =
      std::upper_bound(m_times.begin(), m_times.end(), t) - m_times.begin() -
      1;
    const double h = m_times[i + 1] - m_times[i];
    const double s = (t - m_times[i]) / h;
    const double h00 = (1 + 2 * s) * (1 - s) * (1 - s);
    const double h10 = s * (1 - s) * (1 - s);
    const double h01 = s * s * (3 - 2 * s);
    const double h11 = s * s * (s - 1);
    for (int c = 0; c < Channels; ++c) {
      v[c] = h00 * m_values[i][c] + h10 * h * m_slopes[i][c] +
             h01 * m_values[i + 1][c] + h11 * h * m_slopes[i + 1][c];
    }
  }

  double direction[3], level[3], side[3], position[3], up[3];
  frame(v[Azimuth], v[Elevation], direction, level);
  vtkMath::Cross(direction, level, side);
  for (int k = 0; k < 3; ++k) {
    position[k] = v[FocalX + k] + v[Distance] * direction[k];
    up[k] = std::cos(v[Roll]) * level[k] + std::sin(v[Roll]) * side[k];
  }
  camera->SetFocalPoint(v[FocalX], v[FocalY], v[FocalZ]);
  camera->SetPosition(position);
  camera->SetViewUp(up);
  camera->SetViewAngle(v[ViewAngle]);
  camera->SetParallelScale(v[ParallelScale]);
}

} // namespace tomviz
