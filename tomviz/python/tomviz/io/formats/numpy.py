# -*- coding: utf-8 -*-

###############################################################################
#
#  This source file is part of the tomviz project.
#
#  Copyright Kitware, Inc.
#
#  This source code is released under the New BSD License, (the "License").
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###############################################################################

import numpy as np

from tomviz.io import FileType, IOBase, Reader, Writer

import tomviz.utils

from vtk import vtkImageData


class NumpyBase(IOBase):

    def __init__(self):
        super(NumpyBase, self).__init__('binary')

    @staticmethod
    def file_type():
        return FileType('NumPy binary format', ['npy'])


class NumpyWriter(Writer, NumpyBase):
    def __init__(self):
        super(NumpyWriter, self).__init__()

    def write(self, path, data_object):
        data = tomviz.utils.get_array(data_object)
        with self.open(path) as f:
            np.save(f, data)


class NumpyReader(Reader, NumpyBase):
    def __init__(self):
        super(NumpyReader, self).__init__()

    def read(self, path):
        print(path)
        with self.open(path) as f:
            data = np.load(f)

        image_data = vtkImageData()
        (x, y, z) = data.shape

        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
        tomviz.utils.set_array(image_data, data)

        return image_data
