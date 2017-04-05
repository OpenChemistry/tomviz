from __future__ import absolute_import
import mock

constants = mock.MagicMock()
Dispatch = mock.MagicMock()
tia = mock.MagicMock()
find_display = tia.ActiveDisplayWindow.return_value.FindDisplay.return_value
find_display.SpatialUnit.unitstring = 'nm'
find_display.image.calibration.deltaX = 1.2e-8
find_display.image.calibration.deltaY = 1.2e-8
Dispatch.return_value = tia
