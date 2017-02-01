from tomviz.acquisition import AbstractSource
from . import mock_api


class ApiAdapter(AbstractSource):

    def set_tilt_angle(self, angle):
        return mock_api.set_tilt_angle(angle)

    def preview_scan(self):
        return mock_api.preview_scan()

    def stem_acquire(self):
        return mock_api.stem_acquire()
