# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

# Amira and Avizo (Thermo Fisher) AmiraMesh files (.am). There is no
# public specification; this follows the open readers in ahds (EMDB) and
# natverse's nat, and was checked against their test files:
#
# * A text header: "# AmiraMesh BINARY-LITTLE-ENDIAN 2.1" (or "# Avizo",
#   "BINARY" for big endian, "ASCII"), "define Lattice nx ny [nz]", and in
#   Parameters a BoundingBox "xmin xmax ymin ymax zmin zmax" from the
#   first to the last voxel centre and CoordType "uniform".
# * Data sections declared as "Lattice { float Data } @1", optionally
#   "@1(HxZip,length)" (zlib) or "@1(HxByteRLE,length)". The data follows
#   a line "@1", x varying fastest. The first section is read: in a label
#   field that is the labels.

import re
import zlib

import numpy as np

from tomviz.io import FileType, IOBase, Reader

import tomviz.internal_utils

from vtk import vtkImageData

_HEADER = re.compile(r'# (?:AmiraMesh|Avizo|HyperMesh)(?: 3D)? '
                     r'(BINARY-LITTLE-ENDIAN|BINARY|ASCII)')
_LATTICE = re.compile(
    rb'^[ \t]*(?:define[ \t]+Lattice|nLattice)[ \t]+(\d+)[ \t]+(\d+)'
    rb'(?:[ \t]+(\d+))?[ \t]*\r?$', re.M)
_SECTION = re.compile(
    rb'^[ \t]*Lattice[ \t]*\{[ \t]*(\w+)[ \t]*(\[\d+\])?[ \t]+\w+[ \t]*\}'
    rb'[ \t]*=?[ \t]*@(\d+)(?:\([ \t]*(\w+)[ \t]*,[ \t]*(\d+)[ \t]*\))?',
    re.M)
_DATA_START = re.compile(rb'^@1[ \t]*\r?$', re.M)
_BOUNDING_BOX = re.compile(rb'^[ \t]*BoundingBox[ \t]+([-+.\deE \t]+)')
_COORD_TYPE = re.compile(rb'^\s*CoordType "(\w+)"', re.M)
# As ahds and nat read them; "byte" is unsigned.
_DTYPES = {b'byte': 'u1', b'ubyte': 'u1', b'short': 'i2', b'ushort': 'u2',
           b'int': 'i4', b'uint': 'u4', b'long': 'i8', b'ulong': 'u8',
           b'uint64': 'u8', b'float': 'f4', b'double': 'f8'}


def _decode_rle(data, size):
    """HxByteRLE: a count byte n, then n literal bytes if n > 127 (minus
    128), else one byte repeated n times. The loop is over runs, so it is
    quick for label fields (long runs) and slow for noisy data."""
    data = bytes(data)
    out = bytearray()
    i = 0
    while len(out) < size and i < len(data):
        n = data[i]
        if n > 127:
            n -= 128
            out += data[i + 1:i + 1 + n]
            i += 1 + n
        else:
            out += data[i + 1:i + 2] * n
            i += 2
    if len(out) != size:
        raise ValueError('HxByteRLE data is shorter than the lattice')
    return out


def _refuse(message):
    print(f'Tomviz cannot open this Amira file: {message}')
    return vtkImageData()


def _read_values(data, mode, kind, encoding, length, count):
    """The first data section's values, in file order, or None. ``data``
    is a memoryview of the file from the start of the section."""
    dtype = np.dtype(_DTYPES[kind]).newbyteorder(
        '<' if mode == 'BINARY-LITTLE-ENDIAN' else '>')
    native = dtype.newbyteorder('=')
    if mode == 'ASCII':
        values = np.array(bytes(data).split(None, count)[:count],
                          dtype=float)
        return values.astype(native) if len(values) == count else None
    size = count * dtype.itemsize
    try:
        if encoding == b'HxZip':
            raw = zlib.decompress(data[:int(length)])
        elif encoding == b'HxByteRLE':
            raw = _decode_rle(data[:int(length)], size)
        else:
            raw = data[:size]
    except (zlib.error, ValueError):
        return None
    if len(raw) < size:
        return None
    # A view of the file's bytes; only big endian data is copied.
    return np.frombuffer(raw, dtype, count).astype(native, copy=False)


def _bounding_box(header):
    """The BoundingBox of the top-level Parameters block, or None."""
    depth = 0
    for line in header.splitlines():
        match = _BOUNDING_BOX.match(line)
        if match and depth == 1:
            return [float(v) for v in match.group(1).split()[:6]]
        depth += line.count(b'{') - line.count(b'}')
    return None


def _set_geometry(image_data, header, dims):
    """Origin and spacing from the BoundingBox, which spans the voxel
    centres; with one voxel along an axis, Amira takes the box size as the
    voxel size."""
    bounds = _bounding_box(header)
    if not bounds or len(bounds) != 6:
        return
    spacing = []
    for n, low, high in zip(dims, bounds[0::2], bounds[1::2]):
        step = (high - low) / (n - 1) if n > 1 else high - low
        spacing.append(step if step > 0 else 1.0)
    image_data.SetOrigin(bounds[0], bounds[2], bounds[4])
    image_data.SetSpacing(spacing)


class AmiraBase(IOBase):

    @staticmethod
    def file_type():
        return FileType('Amira/Avizo AmiraMesh', ['am'])


class AmiraReader(Reader, AmiraBase):

    def read(self, path):
        with open(path, 'rb') as f:
            content = f.read()

        first = content[:200].split(b'\n', 1)[0].decode('latin-1')
        mode = _HEADER.match(first)
        start = _DATA_START.search(content)
        if not mode or not start:
            return _refuse('not an AmiraMesh file with data')
        header = content[:start.start()]

        lattice = _LATTICE.search(header)
        sections = _SECTION.findall(header)
        coord_type = _COORD_TYPE.search(header)
        if not lattice:
            return _refuse('it has no lattice (a surface or another '
                           'object)')
        if not sections:
            return _refuse('it declares no lattice data')
        if coord_type and coord_type.group(1) != b'uniform':
            return _refuse(f'its grid is {coord_type.group(1).decode()}, '
                           'not uniform')
        kind, components, number, encoding, length = sections[0]
        if number != b'1' or components:
            return _refuse('its first data section is not a scalar field')
        if kind not in _DTYPES:
            return _refuse(f'it holds {kind.decode()} data')

        nx, ny, nz = (int(v or 1) for v in lattice.groups())
        view = memoryview(content)[start.end() + 1:]
        data = _read_values(view, mode.group(1), kind, encoding, length,
                            nx * ny * nz)
        if data is None:
            return _refuse('its data is damaged or shorter than the lattice')
        # x varies fastest: (z, y, x) in C order is (x, y, z) in Fortran.
        data = np.transpose(data.reshape((nz, ny, nx)), (2, 1, 0))

        image_data = vtkImageData()
        image_data.SetOrigin(0, 0, 0)
        image_data.SetSpacing(1, 1, 1)
        image_data.SetExtent(0, nx - 1, 0, ny - 1, 0, nz - 1)
        tomviz.internal_utils.set_array(image_data, data)
        _set_geometry(image_data, header, (nx, ny, nz))
        return image_data
