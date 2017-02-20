from tomviz.acquisition import AbstractSource
from tomviz.acquisition import describe
from .mock import TemScripting



class FeiAdapter(AbstractSource):

    def __init__(self):
        self._connected = False

    def connect(self, **params):
        self._microscope = TemScripting.Microscope()
        self._microscope.Connect()

        self._acq = self._microscope.m_temScripting.Acquisition
        self._proj = self._microscope.m_temScripting.Projection
        self._ill = self._microscope.m_temScripting.Illumination
        self._detector = self._acq.Detectors[0]
        self._microscope.m_temScripting.Acquisition.AddAcqDevice(self._detector)
        self._connected = True

    @describe([{
        'name': 'stem_rotation',
        'label': 'Stem Rotation',
        'description': 'The STEM rotation angle (in radians).',
        'type': 'double',
        'default': 0.0
    }])
    def tilt_params(self, stem_rotation=0.0):
        if not self._connected:
            raise Exception('Source not connected.')

        self._ill.StemRotation = stem_rotation

        return self._ill.StemRotation

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
        'type' : 'enumeration',
        'default' : 0,
        'options' : [{
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
    def acquisition_params(self, binning=None, image_size=None, dwell_time=None):
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
        print "TEST: %s" %  self._acq.Detectors.AcqParams.Binning
        return {
            'binning': self._acq.Detectors.AcqParams.Binning,
            'image_size': self._acq.Detectors.AcqParams.ImageSize,
            'dwell_time':self._acq.Detectors.AcqParams.DwellTime
        }

    def stem_acquire(self, **params):
        if not self._connected:
            raise Exception('Source not connected.')

        image_set = self._acq.AcquireImages()
        image = image_set[0]

        return bytes(image.AsSafeArray)
