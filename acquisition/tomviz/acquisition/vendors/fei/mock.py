from __future__ import absolute_import
import mock
import requests

from PIL import Image

# Get the test data
response = requests.get(
    'https://data.kitware.com/api/v1/file/5893921d8d777f07219fca7e/download',
    stream=True)
img = Image.open(response.raw)


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


