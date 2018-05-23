from abc import ABCMeta, abstractmethod


class FileType(object):
    """
    Container class for file type information.
    """
    def __init__(self, display_name=None, extensions=None):
        self.display_name = display_name
        self.extensions = extensions


@six.add_metaclass(ABCMeta)
class IOBase(object):

    def __init__(self):
        self._file_type = None

    @staticmethod
    @abstractmethod
    def file_type():
        """
        :returns An instance of the FileFormat class. This is used to associate
        a file type with a reader.
        :rtype tomviz.io.FileType
        """

    @abstractmethod
    def open(file_paths):
        """
        :param file_paths: The list of files to open.
        :type params: list
        :returns: ...
        """

    def close(self):
        pass
