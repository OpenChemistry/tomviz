from tomviz.acquisition import AbstractSource
from . import mock_api
from tomviz.acquisition import describe


class ApiAdapter(AbstractSource):

    def connect(self, **params):
        return mock_api.connect()

    def disconnect(self, **params):
        return mock_api.disconnect()

    def tilt_params(self, angle):
        return mock_api.set_tilt_angle(angle)

    @describe([{
        'name': 'test',
        'label': 'test',
        'description': 'Test params.',
        'type': 'int',
        'default': 1
    },{
        'name': 'foo',
        'label': 'Foo',
        'description': 'Foo bar',
        'type': 'string',
        'default': 'foo'
    }])
    def acquisition_params(self, **params):
        return mock_api.set_acquisition_params(**params)

    def preview_scan(self):
        return mock_api.preview_scan()

    def stem_acquire(self):
        return mock_api.stem_acquire()
