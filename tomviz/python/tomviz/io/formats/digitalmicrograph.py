# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

# Gatan DigitalMicrograph files (.dm3, .dm4), read with tomviz.io.dm.

import numpy as np

from tomviz.io import FileType, IOBase, Reader
from tomviz.io import dm

import tomviz.internal_utils

from vtk import vtkImageData, vtkStringArray

_DEGREES = ('deg', 'degree', 'degrees', '\u00b0')


def _calibration(dataset, ndim):
    """(scale, origin, unit) per axis of the data array, in its order."""
    scales = list(np.atleast_1d(dataset.get('pixelSize', [])))
    origins = list(np.atleast_1d(dataset.get('pixelOrigin', [])))
    units = list(dataset.get('pixelUnit', []))
    n = max(len(scales), len(origins), len(units))
    calibration = []
    for i in range(n):
        calibration.append(
            (float(scales[i]) if i < len(scales) else 1.0,
             float(origins[i]) if i < len(origins) else 0.0,
             str(units[i]).strip() if i < len(units) else ''))
    # A stack may carry a calibration for its images only, not for the
    # stack axis; the missing entries are the leading ones.
    calibration = [(1.0, 0.0, '')] * max(ndim - n, 0) + calibration
    return calibration[-ndim:]


def _length_unit(unit):
    # Plain ASCII for VTK and Qt ('um' is also the OME-TIFF reader's
    # default unit).
    return unit.replace('\u00b5', 'u').replace('\u03bc', 'u')


class DigitalMicrographBase(IOBase):

    @staticmethod
    def file_type():
        return FileType('Gatan DigitalMicrograph', ['dm3', 'dm4'])


class DigitalMicrographReader(Reader, DigitalMicrographBase):

    def read(self, path):
        # on_memory=False reads the data instead of memory-mapping the
        # file, so the file can be closed while the array is in use.
        with dm.FileDM(path, on_memory=False) as f:
            images = f.numObjects - (1 if f.thumbnail else 0)
            try:
                dataset = f.getDataset(0)
            except OSError as e:
                # dm.py has no numpy type for RGB and bool data.
                if 'Unsupported binary data type' not in str(e):
                    raise
                print('Tomviz cannot open this DigitalMicrograph data type '
                      '(RGB or binary)')
                return vtkImageData()
        if images > 1:
            print(f'This DigitalMicrograph file has {images} images; '
                  'opening the first')
        data = dataset['data']

        if data.ndim not in (2, 3) or data.dtype.kind == 'c':
            print('Tomviz can only open DigitalMicrograph images and stacks '
                  f'of real numbers, not {data.ndim}D {data.dtype} data')
            return vtkImageData()

        calibration = _calibration(dataset, data.ndim)
        if data.ndim == 2:
            data = data[np.newaxis]
            calibration.insert(0, (1.0, 0.0, ''))
        (scale_z, origin_z, unit_z), (scale_y, _, unit_y), \
            (scale_x, _, unit_x) = calibration

        # (image, row, column) to (column, row, image), the layout VTK
        # reads a TIFF of the same images in. The transpose is a view.
        data = np.transpose(data, (2, 1, 0))

        image_data = vtkImageData()
        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        x, y, z = data.shape
        image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
        tomviz.internal_utils.set_array(image_data, data)

        unit = _length_unit(unit_x)
        if (unit and unit == _length_unit(unit_y) and
                np.isfinite([scale_x, scale_y]).all() and
                scale_x > 0 and scale_y > 0):
            spacing_z = scale_x
            if (_length_unit(unit_z) == unit and np.isfinite(scale_z) and
                    scale_z > 0):
                spacing_z = scale_z
            image_data.SetSpacing(scale_x, scale_y, spacing_z)
            units = vtkStringArray()
            units.SetName('units')
            units.SetNumberOfValues(3)
            for i in range(3):
                units.SetValue(i, unit)
            image_data.GetFieldData().AddArray(units)

        # A stack calibrated in degrees, as Gatan's tomography acquisition
        # writes it, is a tilt series.
        if unit_z.lower() in _DEGREES:
            angles = (np.arange(z) - origin_z) * scale_z
            tomviz.internal_utils.set_tilt_angles(image_data,
                                                  angles.astype(np.float64))

        return image_data
