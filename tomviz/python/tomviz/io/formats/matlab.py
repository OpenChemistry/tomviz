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
import scipy.io

from tomviz.io import FileType, IOBase, Reader

import tomviz.utils

from vtk import vtkImageData


class MatlabBase(IOBase):

    @staticmethod
    def file_type():
        return FileType('MATLAB binary format', ['mat'])


class MatlabReader(Reader, MatlabBase):

    def read(self, path):
        mat_dict = scipy.io.loadmat(path)

        data = None
        for key, item in mat_dict.items():
            # Assume only one array per file
            if isinstance(item, np.ndarray):
                data = item
                break

        if data is None:
            return vtkImageData()

        image_data = vtkImageData()
        (x, y, z) = data.shape

        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
        tomviz.utils.set_array(image_data, data)

        return image_data
