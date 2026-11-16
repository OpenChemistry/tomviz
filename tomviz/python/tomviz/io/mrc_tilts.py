# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Tilt angles for MRC tilt series (.mrc, .st, .ali).

The MRC format has no tilt angle field. Acquisition and processing
programs put the angles in one of these places, tried in this order:

* ``<name>.tlt`` next to an aligned stack (``.ali``): IMOD's refined
  angles, which belong to the aligned images.
* The extended header:

  - ``FEI1`` / ``FEI2`` (Thermo Fisher, MRC2014 specification 306687):
    one metadata block per image, "Alpha tilt" in degrees as a float64
    at byte 100, in the file's byte order, present when bit 7 of the
    little-endian "Bitmask 1" at byte 8 is set.
  - ``SERI`` (SerialEM): ``nint`` bytes per image, with the fields
    flagged in ``nreal``; with bit 1 set the first is the tilt angle in
    degrees times 100 as an int16 (IMOD's mrc_format.txt).
  - The older FEI header (128 bytes per image, ``nint`` = 0 and
    ``nreal`` = 32): the tilt angle as a float32 at byte 0 of each block.

* A SerialEM or Thermo Fisher autodoc, ``<file>.mdoc`` or
  ``<name>.mdoc``: ``TiltAngle = ...`` in each ``[ZValue = n]`` section.
* IMOD's ``<name>.rawtlt`` (the raw angles) or ``<name>.tlt``.

Only a list of finite angles, exactly one per image, is used, and only
if the angles differ: headers are written for every acquisition, and a
stack of frames at one angle (usually 0) is not a tilt series.
"""

import math
import os
import re
import struct


def tilt_angles(path, count):
    """Tilt angles in degrees for the ``count`` images of an MRC stack,
    or None when none are found."""
    stem = os.path.splitext(path)[0]
    sources = []
    if path.lower().endswith('.ali'):
        sources.append(lambda: _text_angles(stem + '.tlt'))
    sources += [
        lambda: header_angles(path),
        lambda: mdoc_angles(path + '.mdoc'),
        lambda: mdoc_angles(stem + '.mdoc'),
        lambda: _text_angles(stem + '.rawtlt'),
        lambda: _text_angles(stem + '.tlt'),
    ]
    for source in sources:
        try:
            angles = source()
        except (OSError, ValueError, struct.error):
            angles = None
        if (angles is not None and len(angles) == count and
                all(math.isfinite(a) for a in angles) and
                len(set(angles)) > 1):
            return [float(a) for a in angles]
    return None


def _text_angles(path):
    if not os.path.exists(path):
        return None
    values = []
    with open(path) as f:
        for line in f:
            values += [float(v) for v in line.split()]
    return values or None


def mdoc_angles(path):
    """TiltAngle per [ZValue = n] section of a SerialEM autodoc."""
    if not os.path.exists(path):
        return None
    angles = {}
    z = None
    with open(path, errors='replace') as f:
        for line in f:
            line = line.strip()
            m = re.match(r'\[\s*ZValue\s*=\s*(\d+)\s*\]', line)
            if m:
                z = int(m.group(1))
                continue
            if line.startswith('['):
                z = None
                continue
            key, _, value = line.partition('=')
            if z is not None and key.strip() == 'TiltAngle' and value.split():
                angles[z] = float(value.split()[0])
    if not angles or sorted(angles) != list(range(len(angles))):
        return None
    return [angles[z] for z in range(len(angles))]


def header_angles(path):
    """Tilt angles from the extended header, or None."""
    with open(path, 'rb') as f:
        header = f.read(1024)
        if len(header) < 1024:
            return None
        # Machine stamp: 0x44 0x44 (or 0x44 0x41) little endian, 0x11 0x11
        # big endian. Old files may leave it zero: assume little endian.
        bo = '>' if header[212] == 0x11 else '<'
        nz = struct.unpack(bo + 'i', header[8:12])[0]
        nsymbt = struct.unpack(bo + 'i', header[92:96])[0]
        exttyp = header[104:108]
        nint, nreal = struct.unpack(bo + '2h', header[128:132])
        if nsymbt <= 0 or nz <= 0 or \
                nsymbt > os.fstat(f.fileno()).st_size - 1024:
            return None

        def read(size):
            # Only what the angles need, never more than the header.
            if size > nsymbt:
                return None
            f.seek(1024)
            return f.read(size)

        if exttyp in (b'FEI1', b'FEI2'):
            return _fei_angles(read, nz, bo)
        if exttyp == b'SERI':
            return _serialem_angles(read, nz, nint, nreal, bo)
        if (exttyp in (b'\0\0\0\0', b'    ', b'MRCO', b'AGAR') and
                nreal == 32 and nint == 0):
            return _old_fei_angles(read, nz, bo)
    return None


# Bit of "Bitmask 1" that says "Alpha tilt" is set (MRC2014 spec).
_FEI_ALPHA_TILT = 1 << 7


def _fei_angles(read, nz, bo):
    # Every block is as long as the first one says ("Metadata size").
    first = read(4)
    if first is None or len(first) < 4:
        return None
    size = struct.unpack(bo + 'i', first)[0]
    ext = read(size * nz) if size >= 108 else None
    if ext is None or len(ext) < size * nz:
        return None
    angles = []
    for i in range(nz):
        # The bitmask is little-endian whatever the file's byte order.
        bitmask = struct.unpack('<I', ext[i * size + 8:i * size + 12])[0]
        if not bitmask & _FEI_ALPHA_TILT:
            return None
        angles.append(
            struct.unpack(bo + 'd', ext[i * size + 100:i * size + 108])[0])
    return angles


# SerialEM extended header fields, in order: (bit in nreal, bytes),
# including the bits IMOD reserves for future 2- and 4-byte items.
_SERI_FIELDS = ((1, 2), (2, 6), (4, 4), (8, 2), (16, 2), (32, 4),
                (64, 2), (128, 4), (256, 2), (512, 4), (1024, 2))


def _serialem_angles(read, nz, nint, nreal, bo):
    if nint <= 0 or not nreal & 1:
        return None
    flagged = sum(n for bit, n in _SERI_FIELDS if nreal & bit)
    if flagged != nint:
        # Then nint and nreal are counts of ints and reals, not SerialEM.
        return None
    ext = read(nint * nz)
    if ext is None or len(ext) < nint * nz:
        return None
    return [struct.unpack(bo + 'h', ext[i * nint:i * nint + 2])[0] / 100.0
            for i in range(nz)]


def _old_fei_angles(read, nz, bo):
    ext = read(128 * nz)
    if ext is None or len(ext) < 128 * nz:
        return None
    return [struct.unpack(bo + 'f', ext[i * 128:i * 128 + 4])[0]
            for i in range(nz)]
