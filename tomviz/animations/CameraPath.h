/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCameraPath_h
#define tomvizCameraPath_h

#include <QList>

#include <array>
#include <vector>

class vtkCamera;

namespace tomviz {

struct Viewpoint;

/// The camera's flight through a stretch of consecutive viewpoints.
///
/// Splining the camera positions cuts inside an arc around the data, so
/// the camera zooms in and out as it swings round. Instead each viewpoint
/// is read as a turntable camera (focal point, distance, azimuth and
/// elevation about a vertical axis, roll, view angle, parallel scale),
/// and each of those is interpolated over the times the viewpoints are
/// reached with a monotone cubic, which never overshoots.
class CameraPath
{
public:
  /// Fly through viewpoints[first..last], viewpoint first + i reached at
  /// times[i]. Times must not decrease; viewpoints sharing a time are a
  /// cut, and the later one counts.
  CameraPath(const QList<Viewpoint>& viewpoints, int first, int last,
             const std::vector<double>& times);

  /// Put @a camera where the path is at time @a t. Parallel projection is
  /// left alone, since switching it mid-path would jump.
  void apply(double t, vtkCamera* camera) const;

private:
  static constexpr int Channels = 9;
  using Values = std::array<double, Channels>;

  /// The unit direction from the focal point to a turntable camera at
  /// @a azimuth and @a elevation, and the view-up that keeps it level.
  void frame(double azimuth, double elevation, double direction[3],
             double level[3]) const;

  std::vector<double> m_times;
  std::vector<Values> m_values;
  std::vector<Values> m_slopes;
  // The turntable's axis, and the horizontal directions azimuth is
  // measured in (e1 x e2 = axis)
  std::array<double, 3> m_axis = { 0, 0, 1 };
  std::array<double, 3> m_e1 = { 1, 0, 0 };
  std::array<double, 3> m_e2 = { 0, 1, 0 };
};

} // namespace tomviz

#endif
