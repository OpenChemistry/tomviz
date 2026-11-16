###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Tests for finding the tilt angles of MRC tilt series."""

from __future__ import annotations

import struct
from pathlib import Path

import numpy as np
import pytest

from tomviz.io import mrc_tilts

ANGLES = [-60.0, -30.0, 0.0, 30.0, 60.0]


def _write_mrc(
    path: Path,
    nz: int,
    exttyp: bytes = b'\0\0\0\0',
    extended: bytes = b'',
    nint: int = 0,
    nreal: int = 0,
    byte_order: str = '<',
) -> Path:
    """A 2 x 2 x nz float32 MRC file with the given extended header."""
    header = bytearray(1024)
    struct.pack_into(byte_order + '4i', header, 0, 2, 2, nz, 2)
    struct.pack_into(byte_order + 'i', header, 92, len(extended))
    header[104:108] = exttyp
    struct.pack_into(byte_order + '2h', header, 128, nint, nreal)
    header[208:212] = b'MAP '
    header[212:216] = (bytes([0x11, 0x11, 0, 0]) if byte_order == '>'
                       else bytes([0x44, 0x44, 0, 0]))
    data = np.zeros((nz, 2, 2), dtype=byte_order + 'f4').tobytes()
    path.write_bytes(bytes(header) + extended + data)
    return path


def _fei_extended(angles: list[float], byte_order: str = '<',
                  size: int = 888, bitmask: int = 0xffffcfff) -> bytes:
    # MRC2014 FEI1/FEI2 header: one block per image, the block size at
    # byte 0, "Bitmask 1" (little-endian; bit 7 says "Alpha tilt" is set)
    # at byte 8 and "Alpha tilt" (degrees, float64) at byte 100. The
    # default bitmask is the one in mrcfile's FEI1 and FEI2 test files.
    blocks = bytearray(size * len(angles))
    for i, angle in enumerate(angles):
        struct.pack_into(byte_order + 'i', blocks, i * size, size)
        struct.pack_into('<I', blocks, i * size + 8, bitmask)
        struct.pack_into(byte_order + 'd', blocks, i * size + 100, angle)
    return bytes(blocks)


def _serialem_extended(angles: list[float], nint: int) -> bytes:
    # SerialEM: nint bytes per image, the tilt angle * 100 (int16) first.
    blocks = bytearray(nint * len(angles))
    for i, angle in enumerate(angles):
        struct.pack_into('<h', blocks, i * nint, int(round(angle * 100)))
    return bytes(blocks)


def _agard_extended(angles: list[float]) -> bytes:
    # Older FEI/Agard header: 128 bytes per image (1024 of them), the tilt
    # angle as float32 first.
    blocks = bytearray(128 * 1024)
    for i, angle in enumerate(angles):
        struct.pack_into('<f', blocks, i * 128, angle)
    return bytes(blocks)


def _mdoc(path: Path, angles: list[float]) -> None:
    lines = ['PixelSpacing = 2.0', '']
    for z, angle in enumerate(angles):
        lines += [f'[ZValue = {z}]', f'TiltAngle = {angle:.2f}', '']
    path.write_text('\n'.join(lines))


@pytest.mark.parametrize('exttyp', [b'FEI1', b'FEI2'])
@pytest.mark.parametrize('byte_order', ['<', '>'])
def test_fei_header_angles(tmp_path: Path, exttyp: bytes,
                           byte_order: str) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 5, exttyp,
                      _fei_extended(ANGLES, byte_order),
                      byte_order=byte_order)
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(ANGLES)


def test_fei_header_without_alpha_tilt_is_not_used(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 5, b'FEI2',
                      _fei_extended(ANGLES, bitmask=0xffffcfff & ~(1 << 7)))
    assert mrc_tilts.tilt_angles(str(path), 5) is None


def test_extended_header_larger_than_the_file(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 5, b'FEI2', _fei_extended(ANGLES))
    data = bytearray(path.read_bytes())
    struct.pack_into('<i', data, 92, 2**31 - 1)
    path.write_bytes(bytes(data))
    assert mrc_tilts.header_angles(str(path)) is None


def test_serialem_header_angles(tmp_path: Path) -> None:
    # Tilt angle, stage position, magnification, intensity and dose
    # (flags 1, 4, 8, 16, 32): 2 + 4 + 2 + 2 + 4 = 14 bytes per image.
    path = _write_mrc(tmp_path / 'a.st', 5, b'SERI',
                      _serialem_extended(ANGLES, 14), nint=14, nreal=61)
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(ANGLES)


def test_serialem_header_with_reserved_fields(tmp_path: Path) -> None:
    # Flags 64 and 128 are reserved for 2- and 4-byte items.
    path = _write_mrc(tmp_path / 'a.st', 5, b'SERI',
                      _serialem_extended(ANGLES, 8), nint=8,
                      nreal=1 | 64 | 128)
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(ANGLES)


def test_ints_and_reals_header_is_not_serialem(tmp_path: Path) -> None:
    # When the flags do not add up to nint, nint and nreal count ints and
    # reals per image instead.
    path = _write_mrc(tmp_path / 'a.st', 5, b'SERI',
                      _serialem_extended(ANGLES, 12), nint=12, nreal=1)
    assert mrc_tilts.tilt_angles(str(path), 5) is None


def test_agard_header_angles(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 5, b'\0\0\0\0',
                      _agard_extended(ANGLES), nint=0, nreal=32)
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(ANGLES)


def test_frames_at_one_angle_are_not_a_tilt_series(tmp_path: Path) -> None:
    # A header (and an autodoc) with the same angle for every image.
    path = _write_mrc(tmp_path / 'a.mrc', 5, b'FEI2',
                      _fei_extended([0.0] * 5))
    _mdoc(tmp_path / 'a.mrc.mdoc', [12.5] * 5)
    assert mrc_tilts.tilt_angles(str(path), 5) is None


def test_mdoc_angles_when_the_header_has_none(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 5, b'FEI2',
                      _fei_extended([0.0] * 5))
    _mdoc(tmp_path / 'a.mrc.mdoc', ANGLES)
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(ANGLES)


def test_mdoc_named_after_the_stack(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.st', 5)
    _mdoc(tmp_path / 'a.mdoc', ANGLES)
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(ANGLES)


def test_mdoc_without_a_value_falls_back_to_rawtlt(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.st', 3)
    (tmp_path / 'a.mdoc').write_text(
        '[ZValue = 0]\nTiltAngle =\n[ZValue = 1]\nTiltAngle = 0\n'
        '[ZValue = 2]\nTiltAngle = 10\n')
    (tmp_path / 'a.rawtlt').write_text('-10\n0\n10\n')
    assert mrc_tilts.tilt_angles(str(path), 3) == pytest.approx(
        [-10.0, 0.0, 10.0])


def test_mdoc_with_a_missing_section_is_not_used(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.st', 3)
    (tmp_path / 'a.mdoc').write_text(
        '[ZValue = 0]\nTiltAngle = -10\n[ZValue = 2]\nTiltAngle = 10\n')
    assert mrc_tilts.tilt_angles(str(path), 3) is None


def test_rawtlt_angles(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 5)
    (tmp_path / 'a.rawtlt').write_text('\n'.join(str(a) for a in ANGLES))
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(ANGLES)


def test_aligned_stack_prefers_refined_tlt(tmp_path: Path) -> None:
    # IMOD's .tlt holds the refined angles of the aligned .ali; the header
    # still has the raw ones.
    refined = [-59.5, -29.8, 0.2, 30.1, 59.9]
    path = _write_mrc(tmp_path / 'a.ali', 5, b'\0\0\0\0',
                      _agard_extended(ANGLES), nint=0, nreal=32)
    (tmp_path / 'a.tlt').write_text('\n'.join(str(a) for a in refined))
    assert mrc_tilts.tilt_angles(str(path), 5) == pytest.approx(refined)


def test_angles_for_another_number_of_images_are_not_used(
        tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 4)
    (tmp_path / 'a.rawtlt').write_text('\n'.join(str(a) for a in ANGLES))
    assert mrc_tilts.tilt_angles(str(path), 4) is None


def test_angles_that_are_not_finite_are_not_used(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 3)
    (tmp_path / 'a.rawtlt').write_text('nan\nnan\nnan\n')
    assert mrc_tilts.tilt_angles(str(path), 3) is None
    (tmp_path / 'a.rawtlt').write_text('1e400\n0\n5\n')
    assert mrc_tilts.tilt_angles(str(path), 3) is None


def test_no_angles(tmp_path: Path) -> None:
    path = _write_mrc(tmp_path / 'a.mrc', 5)
    assert mrc_tilts.tilt_angles(str(path), 5) is None
