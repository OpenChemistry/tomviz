# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

# Thermo Fisher Velox files: HDF5 with the extension .emd, like Berkeley
# EMD files (which Tomviz reads in C++), but with a /Version dataset whose
# JSON says "format": "Velox" (or "DevelopersKit"). The C++ reader hands
# on the .emd files that are not Berkeley EMD. The layout follows the
# Velox reader in RosettaSciIO and was checked against its test files:
#
# * /Data/Image/<id>/Data: the image or stack, shape (y, x, frames).
# * /Data/Image/<id>/Metadata: uint8, shape (n, frames); column i is the
#   NUL-padded JSON of frame i, with BinaryResult.PixelSize (width and
#   height, in BinaryResult.PixelUnitX) and Stage.AlphaTilt (radians).

import json

import h5py
import numpy as np

from tomviz.io import FileType, IOBase, Reader

import tomviz.internal_utils

from vtk import vtkImageData, vtkStringArray

# Pixel size units, in nanometres.
_TO_NM = {'m': 1e9, 'mm': 1e6, 'um': 1e3, '\u00b5m': 1e3, '\u03bcm': 1e3,
          'nm': 1.0, 'A': 0.1, '\u00c5': 0.1, 'pm': 1e-3}


def _is_velox(f):
    if 'Version' not in f:
        return False
    version = np.ravel(f['Version'][()])[0]
    if isinstance(version, bytes):
        version = version.decode('utf-8', 'replace')
    try:
        return json.loads(version).get('format') in ('Velox', 'DevelopersKit')
    except (ValueError, AttributeError):
        return False


def _read_frames(dataset):
    """A (y, x, frames) or (y, x) dataset as an (x, y, frames) Fortran
    array."""
    if dataset.ndim == 2:
        return np.asfortranarray(dataset[()].T[:, :, np.newaxis])
    y, x, n = dataset.shape
    data = np.empty((x, y, n), dtype=dataset.dtype, order='F')
    if dataset.chunks is not None and dataset.chunks[2] == 1:
        # Velox usually stores one frame per chunk, which h5py reads very
        # slowly in one call (h5py issue 977); read it frame by frame.
        for i in range(n):
            data[:, :, i] = dataset[:, :, i].T
    else:
        data[...] = np.transpose(dataset[()], (1, 0, 2))
    return data


def _frame_metadata(metadata, i):
    raw = bytes(np.asarray(metadata[:, i], dtype=np.uint8))
    try:
        return json.loads(raw.rstrip(b'\0').decode('utf-8', 'replace'))
    except ValueError:
        return {}


class VeloxBase(IOBase):

    @staticmethod
    def file_type():
        return FileType('Thermo Fisher Velox', ['emd'])


class VeloxReader(Reader, VeloxBase):

    def read(self, path):
        with h5py.File(path, 'r') as f:
            if not _is_velox(f):
                print('Tomviz cannot open this .emd file: it is neither '
                      'Berkeley EMD nor Velox')
                return vtkImageData()
            images = f.get('Data/Image')
            if images is None or len(images) == 0:
                print('This Velox file has no images (Tomviz does not open '
                      'Velox spectra)')
                return vtkImageData()
            if len(images) > 1:
                print(f'This Velox file has {len(images)} images; opening '
                      'the first')
            group = images[sorted(images)[0]]
            dataset = group['Data']
            if dataset.ndim not in (2, 3) or dataset.dtype.kind not in 'uif':
                print('Tomviz can only open Velox images and stacks of real '
                      f'numbers, not {dataset.ndim}D {dataset.dtype} data')
                return vtkImageData()
            # (y, x, frame) to (x, y, frame), the layout VTK reads a TIFF
            # of the same images in.
            data = _read_frames(dataset)
            metadata = group.get('Metadata')
            frames = ([_frame_metadata(metadata, i)
                       for i in range(metadata.shape[1])]
                      if metadata is not None and metadata.ndim == 2 else [])

        image_data = vtkImageData()
        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        x, y, z = data.shape
        image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
        tomviz.internal_utils.set_array(image_data, data)

        result = frames[0].get('BinaryResult', {}) if frames else {}
        size = result.get('PixelSize', {})
        to_nm = _TO_NM.get(str(result.get('PixelUnitX', '')).strip())
        try:
            width = float(size['width']) * to_nm
            height = float(size.get('height', size['width'])) * to_nm
        except (KeyError, TypeError, ValueError):
            width = height = None
        if (width is not None and np.isfinite([width, height]).all() and
                width > 0 and height > 0):
            image_data.SetSpacing(width, height, width)
            units = vtkStringArray()
            units.SetName('units')
            units.SetNumberOfValues(3)
            for i in range(3):
                units.SetValue(i, 'nm')
            image_data.GetFieldData().AddArray(units)

        # A stack whose frames span at least a degree of alpha tilt is a
        # tilt series; smaller differences are the stage readout of a time
        # series.
        tilts = []
        for frame in frames:
            try:
                tilts.append(float(frame['Stage']['AlphaTilt']))
            except (KeyError, TypeError, ValueError):
                tilts.append(np.nan)
        tilts = np.array(tilts)
        degrees = np.degrees(tilts)
        if (z > 1 and len(tilts) == z and np.isfinite(degrees).all() and
                np.ptp(degrees) >= 1.0):
            tomviz.internal_utils.set_tilt_angles(image_data, degrees)

        return image_data
