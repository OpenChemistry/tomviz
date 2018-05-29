import numpy as np

from tomviz.io import FileType, IOBase, Reader, Writer

import tomviz.utils

from vtk import vtkImageData


class PlainTextBase(IOBase):

    def __init__(self, mode):
        super(PlainTextBase, self).__init__('text')

    @staticmethod
    def file_type():
        return FileType('Plain text dummy format', ['txt'])


class PlainTextWriter(Writer, PlainTextBase):
    def __init__(self):
        super(PlainTextWriter, self).__init__()

    def write(self, path, data_object):
        data = tomviz.utils.get_array(data_object)
        N_PER_LINE = 5
        with self.open(path) as f:
            f.write(" ".join(data.shape))
            data = data.flatten()
            for i in range(len(data)//N_PER_LINE):
                k = i * N_PER_LINE
                f.write(" ".join(data[k:k+N_PER_LINE]))
            f.write(" ".join(data[k+N_PER_LINE:]))


class PlainTextReader(Reader, PlainTextBase):
    def __init__(self):
        super(PlainTextReader, self).__init__()

    def read(self, path):
        with self.open(path) as f:
            is_first = True
            i = 0
            for line in f:
                if is_first:
                    is_first = False
                    size = np.array([int(n) for n in line.split(" ")])
                    if len(size) > 3:
                        raise Exception
                    else:
                        data = np.array(np.product(size))
                else:
                    sub = np.array([float(n) for n in line.split(" ")])
                    k = len(sub)
                    data[i:i+k] = sub
                    i += k

        data.reshape(size)
        image_data = vtkImageData()
        (x, y, z) = data.shape

        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
        tomviz.utils.set_array(image_data, data)

        return image_data
