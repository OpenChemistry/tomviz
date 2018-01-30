from threading import Thread
import datetime
import time
import os
from PIL import Image
from .. import test_image, angle_to_page, test_dm3_tilt_series


class TIFFWriter(Thread):
    def __init__(self, path, delay=1):
        """
        Thread to write TIFF image stack to a particular path. The files are written using the
        following naming convention <timestamp>_<tilt_angle>.tif

        :param path: The path to write the images to.
        :type path: str
        :param delay: The time in seconds to wait between writing images.
        :type delay: int
        """
        super(TIFFWriter, self).__init__()
        self.daemon = True

        self._path = path
        self._delay = delay
        self.img = Image.open(test_image())
        self.series_size = self.img.n_frames

    def run(self):
        for index, angle in enumerate(range(-73, 74, 2)):
            self.img.seek(index)
            timestamp = datetime.datetime.now().strftime('%m_%d_%y_%H_%M_%S')
            filename = '%s_%d.tif' % (timestamp, angle)
            file_path = os.path.join(self._path, filename)
            with open(file_path, 'wb') as fp:
                self.img.save(fp, 'TIFF')
            time.sleep(self._delay)

class DM3Writer(Thread):
    def __init__(self, path, delay=1):
        """
        Thread to write DM3 tilt series to a particular path.

        :param path: The path to write the images to.
        :type path: str
        :param delay: The time in seconds to wait between writing images.
        :type delay: int
        """
        super(DM3Writer, self).__init__()
        self.daemon = True

        self._path = path
        self._delay = delay
        self._files = list(test_dm3_tilt_series())
        self.series_size = len(self._files)

    def run(self):
        for (filename, dm3fp) in self._files:
            file_path = os.path.join(self._path, filename)
            with open(file_path, 'wb') as fp:
                fp.write(dm3fp.read())
            time.sleep(self._delay)
