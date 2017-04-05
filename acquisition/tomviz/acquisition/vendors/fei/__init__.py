import sys
import math
from tomviz.acquisition import AbstractSource
from tomviz.acquisition import describe
import numpy as np
import scipy.misc
import StringIO
import win32com.client
sys.path.append('c:/titan/Scripting')
import TemScripting # noqa

# Add the constants in an easy to access location
TemScripting.constants = win32com.client.constants


class FeiAdapter(AbstractSource):
    def __init__(self):
        self._connected = False

    def connect(self, **params):
        if self._connected:
            return self._connected

        self._microscope = TemScripting.Microscope()
        self._microscope.Connect()
        self._acq = self._microscope.m_temScripting.Acquisition
        self._proj = self._microscope.m_temScripting.Projection
        self._ill = self._microscope.m_temScripting.Illumination
        self._detector = self._acq.Detectors(0)
        self._stage = self._microscope.m_temScripting.Stage
        self._microscope.m_temScripting.Acquisition.AddAcqDevice(
            self._detector)
        self._tia = win32com.client.Dispatch('ESVision.Application')
        self._connected = True

        # Set some defaults
        current_acq_params = self._acq.Detectors.AcqParams
        current_acq_params.Binning = 8
        current_acq_params.ImageSize = 0
        current_acq_params.DwellTime = 6e-6
        self._acq.Detectors.AcqParams = current_acq_params

        return self._connected

    @describe([{
        'name': 'angle',
        'label': 'Angle',
        'description': 'The angle in radian (in radians).',
        'type': 'double',
        'default': 0.0
    }])
    def tilt_params(self, angle=0.0):
        if not self._connected:
            raise Exception('Source not connected.')

        position = self._stage.Position
        position.A = math.radians(angle)
        # StateAxes.axisA = 8
        self._stage.Goto(position, TemScripting.constants.axisA)

        return math.degrees(position.A)

    @describe([{
        'name': 'binning',
        'label': 'Binning',
        'description': 'The binning value to be used for the image '
                       'acquisition. Make sure the value is one of the '
                       'supported binning values.',
        'type': 'int',
        'default': 8
    }, {
        'name': 'image_size',
        'label': 'Image size',
        'description': 'The size of the image to be collected.',
        'type': 'enumeration',
        'default': 0,
        'options': [{
            'Full': 0,
            'Half': 1,
            'Quarter': 2
        }]
    }, {
        'name': 'dwell_time',
        'label': 'Dwell time',
        'description': 'The pixel dwell time in seconds. The frame '
                       'time equals the dwell time times the number '
                       'of pixels plus some overhead (typically '
                       '20%, used for the line flyback). ',
        'type': 'double',
        'default': 12.0
    }])
    def acquisition_params(self, binning=None, image_size=None,
                           dwell_time=None):
        if not self._connected:
            raise Exception('Source not connected.')

        image_size_map = {
            0: self._microscope.ACQIMAGESIZE_FULL,
            1: self._microscope.ACQIMAGESIZE_HALF,
            2: self._microscope.ACQIMAGESIZE_QUARTER
        }

        current_acq_params = self._acq.Detectors.AcqParams

        if binning is not None:
            current_acq_params.Binning = binning

        if image_size is not None:
            image_size = image_size_map[image_size]
            current_acq_params.ImageSize = image_size

        if dwell_time is not None:
            current_acq_params.DwellTime = dwell_time

        # Now update the params
        self._acq.Detectors.AcqParams = current_acq_params

        # TODO: We will need a generic way to extract all params from AcqParams
        return {
            'binning': self._acq.Detectors.AcqParams.Binning,
            'image_size': self._acq.Detectors.AcqParams.ImageSize,
            'dwell_time': self._acq.Detectors.AcqParams.DwellTime,
            'size': self._pixel_size()
        }

    def _stop_acquire(self):
        if self._tia.AcquisitionManager().isAcquiring:
            self._tia.AcquisitionManager().Stop()

    def _pixel_size(self):
        active_window = self._tia.ActiveDisplayWindow()
        # Image display object
        ido = active_window.FindDisplay(active_window.DisplayNames(0))
        unit = ido.SpatialUnit
        unit_name = unit.unitstring
        cal_x = ido.image.calibration.deltaX
        cal_y = ido.image.calibration.deltaY

        return {
            'units': unit_name,
            'calX': cal_x,
            'calY': cal_y
        }

    def preview_scan(self):
        return self.stem_acquire()

    def stem_acquire(self, **params):
        if not self._connected:
            raise Exception('Source not connected.')

        self._stop_acquire()

        image_set = self._acq.AcquireImages()
        image = image_set(0)
        data = np.array(image.AsSafeArray)
        data = np.fliplr(np.rot90(data, 3))
        fp = StringIO.StringIO()
        scipy.misc.imsave(fp, data, 'tiff')

        return fp.getvalue()
