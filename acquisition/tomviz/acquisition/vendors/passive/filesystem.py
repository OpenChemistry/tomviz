import os
import re
try:
    import Queue as queue
except ImportError:
    # py3
    import queue


class Monitor(object):
    def __init__(self, path, filename_regex=None,
                 valid_file_check=lambda f: True):
        super(Monitor, self).__init__()
        self._path = path
        self._files = queue.Queue()
        self._filename_regex \
            = re.compile(filename_regex) if filename_regex else None
        self._listing = set()
        self._valid_file_check = valid_file_check

    def _check(self):

        new_listing = os.listdir(self._path)
        # If we have a regex filter use it
        if self._filename_regex:
            new_listing = filter(lambda f: self._filename_regex.match(f),
                                 new_listing)

        # if we have a valid_file_check use it
        if self._valid_file_check:
            new_listing = filter(lambda f: self._valid_file_check(
                os.path.join(self._path, f)), new_listing)

        # sort by m_time
        new_listing = sorted(new_listing,
                             key=lambda f: os.path.getmtime(
                                 os.path.join(self._path, f)))

        # enqueue an new files
        for f in new_listing:
            if f not in self._listing:
                absolute_path = os.path.join(self._path, f)
                self._files.put(absolute_path)
        self._listing = set(new_listing)

    def get(self):
        """
        Returns the first pending file available. If there are pending files in
        the queue the first in the queue is return, otherwise the path if check
        for new files.

        :returns: The first pending file, None if no file is available.
        """

        if self._files.empty():
            self._check()

        try:
            return self._files.get(block=False)
        except queue.Empty:
            pass

        return None
