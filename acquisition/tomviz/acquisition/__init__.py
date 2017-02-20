from abc import abstractmethod, ABCMeta


def describe(description):
    def wrap(func):
        func.description = description
        return func
    return wrap

class AbstractSource(object):
    """
    Abstract interface implemented to define an acquistion source.
    """
    __metaclass__ = ABCMeta

    def connect(self, **params):
        """
        Connect to the source.

        :param params: The connection parameters.
        :type params: dict
        """

    def disconnect(self, **params):
        """
        Disconnect from the source.

        :param params: The disconnect parameters.
        :type params: dict
        """

    @abstractmethod
    def tilt_params(self, **params):
        """
        Set the tilt angle.

        :param params: The tilt parameters.
        :type params: dict
        :returns: The current tilt parameters.
        """

    def preview_scan(self):
        """
        Peforms a preview scan.
        :returns: The 2D tiff generate by the scan
        """

    @abstractmethod
    def acquisition_params(self, **params):
        """
        Update and fetch the acquisition parameters.
        :param params: The acquisition parameters.
        :type params: dict
        :returns: The current acquisition parameters
        """

    @abstractmethod
    def stem_acquire(self):
        """
        Peforms STEM acquire
        :returns: The 2D tiff generate by the scan
        """

    def requireParams(self, required, provided=None):
        """
        Raises an Exception if the required parameter(s) does not appear in the
        passed parameters.

        :param required: An iterable of required params, or if just one is
            required, you can simply pass it as a string.
        :type required: list, tuple, or str
        :param provided: The list of provided parameters.
        :type provided: dict
        """
        if isinstance(required, str):
            required = (required,)

        for param in required:
            if provided is None or param not in provided:
                raise Exception('Parameter "%s" is required.' % param)


