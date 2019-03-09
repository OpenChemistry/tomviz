# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
import numpy as np

from tomviz.io import FileType, IOBase, Reader, Writer

import tomviz.utils

from vtk import vtkImageData


class NumpyBase(IOBase):

    @staticmethod
    def file_type():
        return FileType('NumPy binary format', ['npy'])


class NumpyWriter(Writer, NumpyBase):

    def write(self, path, data_object):
        data = tomviz.utils.get_array(data_object)

        # Switch to row major order for NPY stores
        data = np.ascontiguousarray(np.transpose(data, [2, 1, 0]))

        with open(path, "wb") as f:
            np.save(f, data)


class NumpyReader(Reader, NumpyBase):

    def read(self, path):
        with open(path, "rb") as f:
            data = np.load(f)

        if len(data.shape) != 3:
            return vtkImageData()

        # NPY stores data as row major order. VTK expects column major order.
        data = np.asfortranarray(np.transpose(data, [2, 1, 0]))

        image_data = vtkImageData()
        (x, y, z) = data.shape

        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
        tomviz.utils.set_array(image_data, data)

        return image_data
