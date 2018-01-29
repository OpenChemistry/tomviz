from threading import Thread
import datetime
import time
import os
from PIL import Image
from .. import test_image, angle_to_page


class Writer(Thread):
    def __init__(self, path, delay=1):
        """
        Thread to writing images to a particular path. The files are written using the
        following naming convention <timestamp>_<tilt_angle>.tif

        :param path: The path to write the images to.
        :type path: str
        :param delay: The time in seconds to wait between writing images.
        :type delay: int
        """
        super(Writer, self).__init__()
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
