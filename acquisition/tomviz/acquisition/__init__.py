from abc import abstractmethod, ABCMeta


class AbstractSource(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def set_tilt_angle(self, angle):
        pass

    @abstractmethod
    def preview_scan(self):
        pass

    @abstractmethod
    def stem_acquire(self):
        pass
