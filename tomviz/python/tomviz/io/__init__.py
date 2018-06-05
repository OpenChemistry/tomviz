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


class FileType(object):
    """
    Container class for file type information.
    """
    def __init__(self, display_name=None, extensions=None):
        self.display_name = display_name
        self.extensions = extensions

    def __str__(self):
        return "%s (%s)" % (self.display_name,
                            " ".join(["*."+ext for ext in self.extensions]))


@six.add_metaclass(ABCMeta)
class IOBase(object):

    @staticmethod
    @abstractmethod
    def file_type():
        """
        :returns An instance of the FileFormat class. This is used to associate
        a file type with a reader.
        :rtype tomviz.io.FileType
        """


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
    def read(self, file):
        """
        :returns Return a vtkDataObject containing the scalars read.
        :param file: the path or file object to read from
        :rtype vtk.vtkDataObject
        """


@six.add_metaclass(ABCMeta)
class Writer(IOBase):
    """
    The base reader class from which writers should be derived.
    """

    @abstractmethod
    def write(self, file, data):
        """
        :param file: the path or file object to write to
        :param data: The data to write to the file.
        :type data: The vtkDataObject instance.
        """
