from __future__ import absolute_import
import mock
from tests.mock import test_image

from PIL import Image

# Get the test data
img = Image.open(test_image())


TemScripting = mock.MagicMock()
microscope = TemScripting.Microscope.return_value
acq = microscope.m_temScripting.Acquisition
proj = microscope.m_temScripting.Projection
ill = microscope.m_temScripting.Illumination

microscope.ACQIMAGESIZE_FULL = 'ACQIMAGESIZE_FULL'
microscope.ACQIMAGESIZE_HALF = 'ACQIMAGESIZE_HALF'
microscope. ACQIMAGESIZE_QUARTER = 'ACQIMAGESIZE_QUARTER'

acq.Detectors.AcqParams.Binning = 10
acq.Detectors.AcqParams.ImageSize = 'FULL'
acq.Detectors.AcqParams.DwellTime = 3.1
image = mock.MagicMock()
image.AsSafeArray = img.tobytes()
acq.AcquireImages.return_value = [image]
