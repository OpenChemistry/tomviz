import tempfile
import os
from tomviz_acquisition.acquisition import AbstractSource


class ApiAdapter1(AbstractSource):

    def connect(self, **params):
        path = os.path.join(tempfile.tempdir, 'adapter_sentinel1')
        with open(path, 'w') as fp:
            fp.write(params['magic'])

    def disconnect(self, **params):
        pass

    def tilt_params(self, angle):
        pass

    def acquisition_params(self, **params):
        pass

    def stem_acquire(self):
        pass
