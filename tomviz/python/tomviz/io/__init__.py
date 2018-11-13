# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
from abc import ABCMeta, abstractmethod


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


class IOBase(object, metaclass=ABCMeta):

    @staticmethod
    @abstractmethod
    def file_type():
        """
        :returns An instance of the FileFormat class. This is used to associate
        a file type with a reader.
        :rtype tomviz.io.FileType
        """


class Reader(IOBase, metaclass=ABCMeta):
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


class Writer(IOBase, metaclass=ABCMeta):
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
