from enum import Enum
from paraview.simple import (
    Render,
    SaveScreenshot
)


class Palette(Enum):
    Current = ""
    TransparentBackground = "TransparentBackground"
    BlackBackground = "BlackBackground"
    DefaultBackground = "DefaultBackground"
    GradientBackground = "GradientBackground"
    GrayBackground = "GrayBackground"
    PrintBackground = "PrintBackground"
    WhiteBackground = "WhiteBackground"


class Camera(object):
    def __init__(self, render_view):
        self._render_view = render_view
        self._camera = render_view.GetActiveCamera()

    def _validate_vec3(self, pos, name):
        if not isinstance(pos, (tuple, list)):
            raise TypeError('%s should be tuple or list' % name)

        if len(pos) != 3:
            raise ValueError('%s must have 3 components' % name)

    def dolly(self, value):
        """
        Divide the camera's distance from the focal point by the given dolly
        value
        """
        self._camera.Dolly(value)
        Render(self._render_view)

    @property
    def roll(self):
        """
        Rotate the camera about the direction of projection.
        """
        return self._camera.GetRoll()

    @roll.setter
    def roll(self, angle):
        self._camera.SetRoll(angle)
        Render(self._render_view)

    def azimuth(self, angle):
        """
        Rotate the camera about the view up vector centered at the focal point.
        """
        self._camera.Azimuth(angle)
        Render(self._render_view)

    def yaw(self, angle):
        """
        Rotate the focal point about the view up vector, using the camera's
        position as the center of rotation.
        """
        self._camera.Yaw(angle)
        Render(self._render_view)

    def elevation(self, angle):
        """
        Rotate the camera about the cross product of the negative of the
        direction of projection and the view up vector, using the focal point as
        the center of rotation.
        """
        self._camera.Elevation(angle)
        Render(self._render_view)

    def pitch(self, angle):
        """
        Rotate the focal point about the cross product of the view up vector and
        the direction of projection, using the camera's position as the center
        of rotation.
        """
        self._camera.Pitch(angle)
        Render(self._render_view)

    @property
    def position(self):
        """
        Set/Get the position of the camera in world coordinates.
        """
        return self._camera.GetPosition()

    @position.setter
    def position(self, pos):
        self._validate_vec3(pos, 'Position')
        [x, y, z] = pos
        self._camera.SetPosition(x, y, z)
        Render(self._render_view)

    @property
    def focal_point(self):
        """
        Set/Get the focal of the camera in world coordinates.
        """
        return self._camera.GetFocalPoint()

    @focal_point.setter
    def focal_point(self, point):
        self._validate_vec3(point, 'Point')
        [x, y, z] = point
        self._camera.SetFocalPoint(x, y, z)
        Render(self._render_view)

    @property
    def view_up(self):
        """
        Set/Get the view up direction for the camera.
        """
        return self._camera.GetViewUp()

    @view_up.setter
    def view_up(self, direction):
        self._validate_vec3(direction, 'Direction')
        [vx, vy, vz] = direction
        self._camera.SetViewUp(vx, vy, vz)
        Render(self._render_view)

    @property
    def distance(self):
        """
        Move the focal point so that it is the specified distance from the
        camera position
        """
        return self._camera.GetDistance()

    @distance.setter
    def distance(self, distance):
        self._camera.SetDistance(distance)
        Render(self._render_view)

    def zoom(self, factor):
        """
        Decrease the view angle by the specified factor. A value greater than 1
        is a zoom-in, a value less than 1 is a zoom-out.
        """
        self._camera.Zoom(factor)
        Render(self._render_view)


class View(object):
    def __init__(self, render_view):
        self._render_view = render_view

    def save_screenshot(self, file_path, palette=Palette.Current, width=-1,
                        height=-1, resolution=None):
        options = {
            'OverrideColorPalette': palette.value
        }

        if width > 0 and height > 0:
            options['ImageResolution'] = (width, height)

        if resolution is not None:
            if not isinstance(resolution, (tuple, list)):
                raise TypeError('Resolution must be a tuple or list')
            if len(resolution) != 2:
                raise ValueError('Resolution must have 2 components')
            options['ImageResolution'] = resolution

        SaveScreenshot(file_path, viewOrLayout=self._render_view, **options)

    @property
    def camera(self):
        return Camera(self._render_view)
