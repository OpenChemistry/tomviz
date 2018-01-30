import mimetypes
import re
from PIL import Image
import struct
import dm3_lib as dm3

from tomviz.acquisition import AbstractSource
from tomviz.acquisition.utility import tobytes
from .filesystem import Monitor

TIFF_MIME_TYPE = 'image/tiff'
DM3_MIME_TYPE = 'image/x-dm3'

# map of mimetype => function to extract metadata and image data from a particular file type.
_extractors = {}

def _dm3_extractor(filepath):
    print '%%%% %s' % filepath
    dm3_file = dm3.DM3(filepath)

    image = Image.fromarray(dm3_file.imagedata)
    image_data = tobytes(image)

    return (dm3_file.info, image_data)

_extractors[DM3_MIME_TYPE] = _dm3_extractor

_valid_file_check_map = {}

def _valid_tiff(filepath):
    try:
        img = Image.open(filepath)
        return img.format == 'TIFF'
    except IOError:
        return False

def _valid_dm3(filepath):
    try:
        dm3.DM3(filepath)
        return True
    except struct.error:
        return False

_valid_file_check_map[TIFF_MIME_TYPE] = _valid_tiff
_valid_file_check_map[DM3_MIME_TYPE] = _valid_dm3

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

    def _extract_filename_metadata(self, filepath):
        match = re.search(self._filename_regex, filepath)
        if match is None:
            return {}

        meta = {}
        for i, group_name in enumerate(self._filename_regex_groups):
            meta[group_name] = match.group(i+1)

        return meta

    def stem_acquire(self):
        """
        Performs STEM acquire. In our case we just fetch an image of the use if
        there is one
        :returns: The 2D tiff generate by the scan along with its meta data.
        """
        file = self._monitor.get()

        # We currently don't have a new image
        if file is None:
            return None

        with open(file) as fp:
            image_data = None
            (self.mimetype, _) = mimetypes.guess_type(file)

            metadata = {}
            if self._filename_regex_groups is not None:
                metadata.update(self._extract_filename_metadata(file))

            if self.mimetype in _extractors:
                extractor = _extractors[self.mimetype]
                (extracted_metadata, image_data) = extractor(file)
                metadata.update(extracted_metadata)

            if image_data is None:
                fp.seek(0)
                image_data = fp.read()

            return (metadata, image_data)
