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
from abc import ABCMeta, abstractmethod
import six

from tomviz.io import IOBase


@six.add_metaclass(ABCMeta)
class Reader(IOBase):
    """
    The base reader class from which readers should be derived.
    """

    """
    Set to True if reader supports loading image stacks.
    """
    supports_stacks = False

    @abstractmethod
    def read(self):
        """
        :returns Return a vtkDataObject containing the scalars read.
        :rtype vtk.vtkDataObject
        """
