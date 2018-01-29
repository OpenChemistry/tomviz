import mimetypes
import re
from PIL import Image

from tomviz.acquisition import AbstractSource
from .filesystem import Monitor

DM3_MIME_TYPE = 'image/x-dm3'

# map of mimetype => function to extract metadata from a particular file type.
metadata_extractors = {}

def _dm3_extractor(file):
    pass

_valid_file_check_map = {

}

def _valid_tiff(filepath):
    try:
        img = Image.open(filepath)
        return img.format == 'TIFF'
    except IOError:
        return False

_valid_file_check_map['image/tiff'] = _valid_tiff

def _valid_file_check(filepath):
    (mimetype, _) = mimetypes.guess_type(filepath)
    if mimetype in _valid_file_check_map:
        return _valid_file_check_map[mimetype](filepath)

    return False


class PassiveWatchSource(AbstractSource):

    def __init__(self):
        self._watcher = None
        self.mimetype = None
        mimetypes.add_type(DM3_MIME_TYPE, '.dm3')

    """
    A simple source that will passively watch a path for files being added to
    a directory.
    """

    def connect(self, path, fileNameRegex=None, fileNameRegexGroups=None,  watchInterval=1,  **params):
        """
        Start the watching thread, to watch for files being added to a directory.

        :param path: The path to the directory to watch for updates.
        :type path: str
        :param fileNameRegex: The regex to use to match filenames.
        """
        self._filename_regex = fileNameRegex
        self._filename_regex_groups = fileNameRegexGroups
        self._monitor = Monitor(path, filename_regex=fileNameRegex, valid_file_check=_valid_file_check)


    def disconnect(self, **params):
        """
        :param params: The disconnect parameters.
        :type params: dict
        """
        pass

    def tilt_params(self, **params):
        """
        N/A - We are in a passive mode so we can't control the tilt parameters.
        TODO: May be this shouldn't be required by the ABC?

        :param params: The tilt parameters.
        :type params: dict
        :returns: The current tilt parameters.
        """
        raise NotImplemented('Not supported by source.')

    def preview_scan(self):
        """
        Peforms a preview scan.
        :returns: The 2D tiff generate by the scan
        """
        pass

    def acquisition_params(self, **params):
        """
        Update and fetch the acquisition parameters.
        :param params: The acquisition parameters.
        :type params: dict
        :returns: The current acquisition parameters
        """
        pass

    def _extract_filename_metadata(self, filename):
        match = re.search(self._filename_regex, filename)
        if match is None:
            return {}

        meta = {}
        for i, group_name in enumerate(self._filename_regex_groups):
            meta[g] = match.group(i)

        return meta

    def stem_acquire(self):
        """
        Peforms STEM acquire. In our case we just fetch an image of the use if
        there is one
        :returns: The 2D tiff generate by the scan along with its meta data.
        """
        file = self._monitor.get()

        # We currently don't have a new image
        if file is None:
            return None

        with open(file) as fp:
            (self.mimetype, _) = mimetypes.guess_type(file)

            metadata = {}
            if self._filename_regex_groups is not None:
                metadata.update(self._extract_filename_metadata())

            if self.mimetype in metadata_extractors:
                extractor = metadata_extractors[self.mimetype]
                metadata.update(extractor(fp))

            fp.seek(0)

            return (metadata, fp.read())
