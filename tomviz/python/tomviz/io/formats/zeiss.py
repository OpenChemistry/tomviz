# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

# ZEISS Xradia files (.txrm projections, .txm volumes, .xrm images) are
# OLE2 compound documents. There is no public specification; the stream
# names follow the ZEISS readers in dxchange and CIL, and were checked
# against a scan from a ZEISS Xradia Versa.

import numpy as np

from tomviz.io import FileType, IOBase, Reader

import tomviz.internal_utils

from vtk import vtkDoubleArray, vtkImageData, vtkIntArray, vtkStringArray

try:
    import olefile
    _HAS_OLEFILE = True
except ImportError:
    _HAS_OLEFILE = False

# ImageInfo/DataType
_DTYPES = {5: np.dtype('<u2'), 10: np.dtype('<f4')}

# ImageInfo/AcquisitionMode of a series of projections (a .txrm)
_PROJECTIONS = 0


def _read_values(ole, name, dtype):
    if not ole.exists(name):
        return None
    data = ole.openstream(name).read()
    return np.frombuffer(data, dtype, len(data) // np.dtype(dtype).itemsize)


def _read_value(ole, name, dtype):
    values = _read_values(ole, name, dtype)
    if values is None or len(values) == 0:
        return None
    return values[0].item()


def _read(ole):
    n = _read_value(ole, 'ImageInfo/NoOfImages', '<u4')
    width = _read_value(ole, 'ImageInfo/ImageWidth', '<u4')
    height = _read_value(ole, 'ImageInfo/ImageHeight', '<u4')
    data_type = _read_value(ole, 'ImageInfo/DataType', '<u4')
    if not n or not width or not height or data_type not in _DTYPES:
        print('Tomviz cannot read this ZEISS file: no images, or an '
              f'unsupported data type ({data_type})')
        return vtkImageData()
    dtype = _DTYPES[data_type]

    mode = _read_value(ole, 'ImageInfo/AcquisitionMode', '<u4')
    angles = _read_values(ole, 'ImageInfo/Angles', '<f4')
    tilt_series = (mode == _PROJECTIONS and n > 1 and angles is not None and
                   len(angles) >= n)
    if mode == _PROJECTIONS and n > 1 and not tilt_series:
        print(f'This ZEISS file has {n} projections but '
              f'{0 if angles is None else len(angles)} angles; opening it '
              'as a volume')

    if tilt_series:
        # The rotation axis runs along the image columns, so each image
        # row is a line of the sinogram. Tomviz reconstructs one sinogram
        # per x, so x is the image row: (row, column, projection).
        data = np.empty((height, width, n), dtype, order='F')
    else:
        # Slices of a volume, or a single image: (column, row, slice),
        # with the rows reversed, as VTK reads ZEISS's own TIFF export of
        # these images (which marks its rows as starting at the bottom).
        # Then a .txrm reconstructed in Tomviz and the .txm ZEISS
        # reconstructed from it are the same object, not mirror images.
        data = np.empty((width, height, n), dtype, order='F')

    for i in range(n):
        # One storage per 100 images: ImageData1/Image1 to Image100, ...
        stream = f'ImageData{i // 100 + 1}/Image{i + 1}'
        frame = np.frombuffer(ole.openstream(stream).read(), dtype,
                              width * height).reshape(height, width)
        data[:, :, i] = frame if tilt_series else frame[::-1].T

    image_data = vtkImageData()
    image_data.SetOrigin(0, 0, 0)
    image_data.SetSpacing(1, 1, 1)
    x, y, z = data.shape
    image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
    tomviz.internal_utils.set_array(image_data, data)

    # ImageInfo/PixelSize is in micrometres.
    pixel_size = _read_value(ole, 'ImageInfo/PixelSize', '<f4')
    if pixel_size is not None and np.isfinite(pixel_size) and pixel_size > 0:
        image_data.SetSpacing(pixel_size, pixel_size, pixel_size)
        units = vtkStringArray()
        units.SetName('units')
        units.SetNumberOfValues(3)
        for i in range(3):
            units.SetValue(i, 'um')
        image_data.GetFieldData().AddArray(units)

    if tilt_series:
        tomviz.internal_utils.set_tilt_angles(
            image_data, angles[:n].astype(np.float64))
        _add_alignment_shifts(ole, image_data, n)

    return image_data


def _add_alignment_shifts(ole, image_data, n):
    # The per-projection shifts ZEISS's software applies (stage drift and
    # the like), in pixels; ZEISS's projection export is each image moved
    # by them, to the nearest pixel. They are kept, not applied: as
    # "alignment_shifts" (x, y) in the tilt series' axes, the distance to
    # move each projection's content, with "alignment_shifts_image_size"
    # (the image size they are in pixels of). The mapping and sign were
    # checked on a Versa scan: moving 100 * image / reference by the
    # rounded shifts gives ZEISS's export to within its rounding.
    x_shifts = _read_values(ole, 'Alignment/X-Shifts', '<f4')
    y_shifts = _read_values(ole, 'Alignment/Y-Shifts', '<f4')
    if x_shifts is None or y_shifts is None or min(
            len(x_shifts), len(y_shifts)) < n:
        return
    shifts = vtkDoubleArray()
    shifts.SetName('alignment_shifts')
    shifts.SetNumberOfComponents(2)
    shifts.SetNumberOfTuples(n)
    for i in range(n):
        # x runs down the image rows, y across the columns.
        shifts.SetTuple2(i, float(y_shifts[i]), float(x_shifts[i]))
    image_data.GetFieldData().AddArray(shifts)
    size = vtkIntArray()
    size.SetName('alignment_shifts_image_size')
    size.SetNumberOfValues(2)
    x, y, _ = image_data.GetDimensions()
    size.SetValue(0, x)
    size.SetValue(1, y)
    image_data.GetFieldData().AddArray(size)


if _HAS_OLEFILE:

    class ZeissBase(IOBase):

        @staticmethod
        def file_type():
            return FileType('ZEISS Xradia', ['txrm', 'txm', 'xrm'])

    class ZeissReader(Reader, ZeissBase):

        def read(self, path):
            with olefile.OleFileIO(path) as ole:
                return _read(ole)
