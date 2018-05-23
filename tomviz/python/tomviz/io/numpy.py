import numpy as np

from tomviz.io.writers import Writer
from tomviz.io.readers import Reader

import tomviz.utils
from tomviz.io import FileType

from vtk import vtkImageData


class NumpyBase(object):
    def __init__(self, mode):
        self._mode = mode

    @staticmethod
    def file_type():
        return FileType('NumPy binary format', ['npy'])

    def open(self, file_path):
        self._file = open(file_path, self._mode)

    def close(self):
        self._file.close()


class NumpyWriter(Writer, NumpyBase):
    def __init__(self):
        super(NumpyWriter, self).__init__('wb')

    def write(self, data_object):
        data = tomviz.utils.get_array(data_object)
        numpy.save(self._file, data)


class NumpyReader(Reader, NumpyBase):
    def __init__(self):
        super(NumpyReader, self).__init__('b')

    def read(self):
        data = np.load(self._file)
        image_data = vtkImageData()
        (x, y, z) = data.shape

        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
        tomviz.utils.set_array(image_data, data)
