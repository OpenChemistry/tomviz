import tempfile
import os
from tomviz_acquisition.acquisition import AbstractSource


class ApiAdapter2(AbstractSource):

    def connect(self, **params):
        path = os.path.join(tempfile.tempdir, 'adapter_sentinel2')
        with open(path, 'w') as fp:
            fp.write(params['magic'])
            fp.write('x2')

    def tilt_params(self, angle):
        pass

    def acquisition_params(self, **params):
        pass

    def stem_acquire(self):
        pass
