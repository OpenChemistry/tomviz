import mimetypes
import re
from PIL import Image
import struct
import os
import dm3_lib as dm3

from tomviz.acquisition import AbstractSource
from tomviz.acquisition import describe
from tomviz.acquisition.utility import tobytes
from .filesystem import Monitor

try:
    dict.iteritems
except AttributeError:
    # Python 3
    def iteritems(d):
        return iter(d.items())
else:
    # Python 2
    def iteritems(d):
        return d.iteritems()


TIFF_MIME_TYPE = 'image/tiff'
DM3_MIME_TYPE = 'image/x-dm3'


# map of mimetype => function to extract metadata and image data from a
# particular file type.
_extractors = {}


def _dm3_extractor(file):
    """
    Extract metadata and image data from a DM3 file.

    :param file: The the DM3 file.
    :type file: The file-like object.
    :return A tuple contain the metadata and image data.
    """
    dm3_file = dm3.DM3(file)
    info = {k: v.decode('utf8') for k, v in dm3_file.info.items()}

    image = Image.fromarray(dm3_file.imagedata)
    image_data = tobytes(image)

    return (info, image_data)


_extractors[DM3_MIME_TYPE] = _dm3_extractor
_valid_file_check_map = {}


def _valid_tiff(filepath):
    """
    Check the TIFF file is valid by opening it.

    :param filepath: The path to the TIFF file.
    :type filepath: str
    """
    try:
        img = Image.open(filepath)
        return img.format == 'TIFF'
    except IOError:
        return False


def _valid_dm3(filepath):
    """
    Check the DM3 file is valid by opening it.

    :param filepath: The path to the TIFF file.
    :type filepath: str
    """
    try:
        dm3.DM3(filepath)
        return True
    except struct.error:
        return False


_valid_file_check_map[TIFF_MIME_TYPE] = _valid_tiff
_valid_file_check_map[DM3_MIME_TYPE] = _valid_dm3


def _valid_file_check(filepath):
    """
    Take a file path and checks that the file is valid according to any
    validation functions registered against it mime type.

    :param filepath: The path to the file.
    :type filepath: str
    :return True, if the validation is successful, False otherwise ( or if there
    if no validation function registered. )
    """
    (mimetype, _) = mimetypes.guess_type(filepath)
    if mimetype in _valid_file_check_map:
        return _valid_file_check_map[mimetype](filepath)

    return False


class PassiveWatchSource(AbstractSource):
    """
    A simple source that will passively watch a path for files being added to
    a directory.
    """

    def __init__(self):
        self._watcher = None
        self.image_data_mimetype = TIFF_MIME_TYPE
        # Register the dm3 mime type.
        mimetypes.add_type(DM3_MIME_TYPE, '.dm3')

    def _validate_connection_params(self, path, fileNameRegex,
                                    fileNameRegexGroups,
                                    groupRegexSubstitutions):

        if not os.path.exists(path):
            raise ValueError("Path '%s does not exist" % path)

        def _validate_regex(regex):
            try:
                re.compile(regex)
            except re.error as err:
                raise ValueError("'%s'is not a valid regex: '%s'"
                                 % (regex, str(err)))

        if fileNameRegex is not None:
            _validate_regex(fileNameRegex)

        if fileNameRegexGroups is not None:
            if fileNameRegex is None:
                raise ValueError("'fileNameRegex' must be provided.")

        if groupRegexSubstitutions is not None:
            if fileNameRegexGroups is None:
                raise ValueError("'fileNameRegexGroups' must be provided.")

            for sub in groupRegexSubstitutions:
                for regex, repl in iteritems(sub):
                    _validate_regex(regex)

            if len(groupRegexSubstitutions) > len(fileNameRegexGroups):
                raise ValueError("The indexes of 'groupRegexSubstitutions'"
                                 " must match 'fileNameRegexGroups'.")

    @describe([{
        'name': 'path',
        'label': 'Watch path',
        'description': 'The path to the directory to watch for updates.',
        'type': 'string'
    }, {
        'name': 'fileNameRegex',
        'label': 'File name regex',
        'description': 'The regex used to match image filename.',
        'type': 'string',
        'default': '.*'
    }])
    def connect(self, path, fileNameRegex=None, fileNameRegexGroups=None,
                groupRegexSubstitutions=None, **params):
        """
        Start the watching thread, to watch for files being added to a
        directory.

        :param path: The path to the directory to watch for updates.
        :type path: str
        :param fileNameRegex: The regex to use to match filenames. May contain
        capture groups.
        :type fileNameRegex: str
        :param fileNameRegexGroups: The names to assign to the capture groups
        extracted from fileNameRegex.
        :type fileNameRegexGroups: str
        :param groupRegexSubstitutions: A list of dictionaries the indexes map
        to the indexes of fileNameRegexGroups. The dictionaries contains
        mappings of regex => repl using to replace parts of the captured group.
        For example {'n': '-'} - The will replace the 'n' with '-'.
        :type groupRegexSubstitutions: str
        """

        self._validate_connection_params(path, fileNameRegex,
                                         fileNameRegexGroups,
                                         groupRegexSubstitutions)
        self._filename_regex = fileNameRegex
        self._filename_regex_groups = fileNameRegexGroups
        self._group_regex_subsitutions = groupRegexSubstitutions
        self._monitor = Monitor(path, filename_regex=fileNameRegex,
                                valid_file_check=_valid_file_check)

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

        # Do we need todo any substitutions?
        if self._group_regex_subsitutions is not None:
            for i, sub in enumerate(self._group_regex_subsitutions):
                group_name = self._filename_regex_groups[i]
                for regex, repl in iteritems(sub):
                    meta[group_name] = re.sub(regex, repl, meta[group_name])

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

        with open(file, 'rb') as fp:
            image_data = None
            (mimetype, _) = mimetypes.guess_type(file)

            metadata = {}
            if self._filename_regex_groups is not None:
                metadata.update(self._extract_filename_metadata(file))

            if mimetype in _extractors:
                extractor = _extractors[mimetype]
                (extracted_metadata, image_data) = extractor(fp)
                metadata.update(extracted_metadata)

            if image_data is None:
                fp.seek(0)
                image_data = fp.read()

            return (metadata, image_data)
