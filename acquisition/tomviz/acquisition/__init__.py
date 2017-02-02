from abc import abstractmethod, ABCMeta


class AbstractSource(object):
    """
    Abstract interface implemented to define an acquistion source.
    """
    __metaclass__ = ABCMeta

    @abstractmethod
    def set_tilt_angle(self, angle):
        """
        Set the tilt angle.
        :param angle: The title angle to set.
        :type angle: int
        :returns: The set tilt angle
        """
        pass

    @abstractmethod
    def preview_scan(self):
        """
        Peforms a preview scan.
        :returns: The 2D tiff generate by the scan
        """
        pass

    @abstractmethod
    def stem_acquire(self):
        """
        Peforms STEM acquire
        :returns: The 2D tiff generate by the scan
        """
        pass
