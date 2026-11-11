###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Tests for DICOM, MRC, ZEISS and Amira I/O format implementations."""

from __future__ import annotations

from collections.abc import Generator
from pathlib import Path
from unittest import mock
import zlib

import numpy as np
import numpy.typing as npt
import pytest

from vtk import vtkImageData
import vtk.numpy_interface.dataset_adapter as dsa
import vtk.util.numpy_support as np_s
from vtkmodules.util.vtkConstants import VTK_DOUBLE


# -- VTK helpers that replicate what tomviz.internal_utils does, without
#    requiring the in-application guard.

def _vtk_set_array(
    dataobject: vtkImageData,
    newarray: npt.NDArray[np.generic],
    name: str = 'Scalars',
) -> None:
    arr = np.asfortranarray(newarray)
    flat = arr.reshape(-1, order='F')
    if flat.dtype not in (np.int8, np.int16, np.uint16, np.float32,
                          np.float64, np.uint8):
        flat = flat.astype(np.float32)
    vtkshape = newarray.shape
    extent = [0, vtkshape[0] - 1, 0, vtkshape[1] - 1, 0, vtkshape[2] - 1]
    dataobject.SetExtent(extent)
    do = dsa.WrapDataObject(dataobject)
    do.PointData.append(flat, name)
    do.PointData.SetActiveScalars(name)


def _vtk_get_array(
    dataobject: vtkImageData,
    name: str | None = None,
    order: str = 'F',
) -> npt.NDArray[np.generic]:
    do = dsa.WrapDataObject(dataobject)
    if name is not None:
        rawarray = do.PointData.GetAbstractArray(name)
    else:
        rawarray = do.PointData.GetScalars()
    scalars = dsa.vtkDataArrayToVTKArray(rawarray, do)
    return np.reshape(scalars, dataobject.GetDimensions(), order=order)


def _vtk_set_tilt_angles(
    dataobject: vtkImageData,
    newarray: npt.NDArray[np.float64],
) -> None:
    vtkarray = np_s.numpy_to_vtk(newarray, deep=1, array_type=VTK_DOUBLE)
    vtkarray.SetName('tilt_angles')
    do = dsa.WrapDataObject(dataobject)
    do.FieldData.RemoveArray('tilt_angles')
    do.FieldData.AddArray(vtkarray)


def _vtk_get_tilt_angles(
    dataobject: vtkImageData,
) -> npt.NDArray[np.float64] | None:
    do = dsa.WrapDataObject(dataobject)
    rawarray = do.FieldData.GetArray('tilt_angles')
    if isinstance(rawarray, dsa.VTKNoneArray):
        return None
    return dsa.vtkDataArrayToVTKArray(rawarray, do)


def _make_image_data(
    arr: npt.NDArray[np.generic],
    spacing: tuple[float, float, float] = (1.0, 1.0, 1.0),
    origin: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> vtkImageData:
    img = vtkImageData()
    img.SetSpacing(*spacing)
    img.SetOrigin(*origin)
    _vtk_set_array(img, arr)
    return img


# Patch tomviz.internal_utils so that the MRC/DICOM writers can call
# get_array, set_array, etc. without needing the full application.
_iu_patch = mock.patch.dict(
    'tomviz.internal_utils.__dict__',
    {
        'get_array': _vtk_get_array,
        'set_array': _vtk_set_array,
        'get_tilt_angles': _vtk_get_tilt_angles,
        'set_tilt_angles': _vtk_set_tilt_angles,
    },
)


@pytest.fixture(autouse=True)
def _patch_internal_utils() -> Generator[None, None, None]:
    with _iu_patch:
        yield


# ---------------------------------------------------------------------------
# MRC writer
# ---------------------------------------------------------------------------

from tomviz.io.formats.mrc import MrcWriter, HEADER_DTYPE


def test_mrc_writer_float32_round_trip(tmp_path: Path) -> None:
    arr = np.random.default_rng(42).random((4, 5, 6)).astype(np.float32)
    path = tmp_path / 'test.mrc'
    MrcWriter().write(str(path), _make_image_data(arr))

    with open(path, 'rb') as f:
        header = np.frombuffer(f.read(1024), dtype=HEADER_DTYPE)[0]
        data = np.frombuffer(f.read(), dtype=np.float32)

    assert header['nx'] == 4
    assert header['ny'] == 5
    assert header['nz'] == 6
    assert header['mode'] == 2  # float32
    assert header['map'] == b'MAP '
    data = data.reshape((6, 5, 4))
    np.testing.assert_allclose(data, np.ascontiguousarray(np.transpose(arr)),
                               atol=1e-7)


def test_mrc_writer_preserves_spacing(tmp_path: Path) -> None:
    arr = np.zeros((3, 4, 5), dtype=np.float32)
    path = tmp_path / 'test.mrc'
    MrcWriter().write(str(path), _make_image_data(arr, spacing=(2.0, 3.0, 4.0)))

    with open(path, 'rb') as f:
        header = np.frombuffer(f.read(1024), dtype=HEADER_DTYPE)[0]

    assert float(header['cella']['x']) == pytest.approx(3 * 2.0)
    assert float(header['cella']['y']) == pytest.approx(4 * 3.0)
    assert float(header['cella']['z']) == pytest.approx(5 * 4.0)


def test_mrc_writer_int16_uses_mode_1(tmp_path: Path) -> None:
    arr = np.arange(24, dtype=np.int16).reshape((2, 3, 4))
    path = tmp_path / 'test.mrc'
    MrcWriter().write(str(path), _make_image_data(arr))

    with open(path, 'rb') as f:
        header = np.frombuffer(f.read(1024), dtype=HEADER_DTYPE)[0]

    assert header['mode'] == 1  # int16


def test_mrc_writer_uint16_uses_mode_6(tmp_path: Path) -> None:
    arr = np.arange(24, dtype=np.uint16).reshape((2, 3, 4))
    path = tmp_path / 'test.mrc'
    MrcWriter().write(str(path), _make_image_data(arr))

    with open(path, 'rb') as f:
        header = np.frombuffer(f.read(1024), dtype=HEADER_DTYPE)[0]

    assert header['mode'] == 6  # uint16


def test_mrc_writer_converts_unsupported_dtype_to_float32(tmp_path: Path) -> None:
    arr = np.arange(24, dtype=np.float64).reshape((2, 3, 4))
    path = tmp_path / 'test.mrc'
    MrcWriter().write(str(path), _make_image_data(arr))

    with open(path, 'rb') as f:
        header = np.frombuffer(f.read(1024), dtype=HEADER_DTYPE)[0]
        data = np.frombuffer(f.read(), dtype=np.float32)

    assert header['mode'] == 2  # float32
    data = data.reshape((4, 3, 2))
    np.testing.assert_allclose(data,
                               np.ascontiguousarray(
                                   np.transpose(arr.astype(np.float32))),
                               atol=1e-7)


def test_mrc_writer_statistics(tmp_path: Path) -> None:
    arr = np.array([[[1.0, 2.0, 3.0, 4.0, 5.0]]], dtype=np.float32)
    path = tmp_path / 'test.mrc'
    MrcWriter().write(str(path), _make_image_data(arr))

    with open(path, 'rb') as f:
        header = np.frombuffer(f.read(1024), dtype=HEADER_DTYPE)[0]

    assert float(header['dmin']) == pytest.approx(1.0)
    assert float(header['dmax']) == pytest.approx(5.0)
    assert float(header['dmean']) == pytest.approx(3.0)
    assert float(header['rms']) == pytest.approx(float(arr.std()), rel=1e-5)


# ---------------------------------------------------------------------------
# DICOM reader/writer
# ---------------------------------------------------------------------------

try:
    import itk
    _HAS_ITK = True
except ImportError:
    _HAS_ITK = False


@pytest.mark.skipif(not _HAS_ITK, reason='ITK not available')
class TestDicom:

    def test_dicom_writer_reader_uint16_round_trip(self, tmp_path: Path) -> None:
        from tomviz.io.formats.dicom import DicomReader, DicomWriter

        arr = np.random.default_rng(7).integers(0, 65535, (4, 5, 6),
                                                 dtype=np.uint16)
        img = _make_image_data(arr, spacing=(1.5, 2.0, 2.5))
        path = str(tmp_path / 'test.dcm')
        DicomWriter().write(path, img)

        result = DicomReader().read(path)
        result_arr = _vtk_get_array(result)

        assert result_arr.shape == arr.shape
        np.testing.assert_array_equal(result_arr, arr)

        result_spacing = result.GetSpacing()
        assert result_spacing[0] == pytest.approx(1.5)
        assert result_spacing[1] == pytest.approx(2.0)
        assert result_spacing[2] == pytest.approx(2.5)

    def test_dicom_writer_reader_uint8_round_trip(self, tmp_path: Path) -> None:
        from tomviz.io.formats.dicom import DicomReader, DicomWriter

        arr = np.random.default_rng(8).integers(0, 255, (3, 4, 5),
                                                 dtype=np.uint8)
        img = _make_image_data(arr)
        path = str(tmp_path / 'test.dcm')
        DicomWriter().write(path, img)

        result = DicomReader().read(path)
        result_arr = _vtk_get_array(result)
        assert result_arr.shape == arr.shape
        np.testing.assert_array_equal(result_arr, arr)

    def test_dicom_writer_converts_float_to_uint16(self, tmp_path: Path) -> None:
        from tomviz.io.formats.dicom import DicomReader, DicomWriter

        arr = np.array([[[0.0, 0.5, 1.0]]], dtype=np.float32)
        img = _make_image_data(arr)
        path = str(tmp_path / 'test.dcm')
        DicomWriter().write(path, img)

        result = DicomReader().read(path)
        result_arr = _vtk_get_array(result)
        assert result_arr.dtype == np.uint16
        # 0.0 -> 0, 0.5 -> ~32767, 1.0 -> 65535
        assert result_arr.flat[0] == 0
        assert result_arr.flat[2] == 65535
        assert abs(int(result_arr.flat[1]) - 32767) < 2

    def test_dicom_preserves_tilt_angles(self, tmp_path: Path) -> None:
        from tomviz.io.formats.dicom import DicomReader, DicomWriter

        arr = np.zeros((3, 3, 3), dtype=np.uint16)
        img = _make_image_data(arr)
        angles = np.array([-60.0, 0.0, 60.0])
        _vtk_set_tilt_angles(img, angles)

        path = str(tmp_path / 'test.dcm')
        DicomWriter().write(path, img)

        result = DicomReader().read(path)
        result_angles = _vtk_get_tilt_angles(result)
        assert result_angles is not None
        np.testing.assert_allclose(result_angles, angles)

    def test_dicom_reader_handles_no_tilt_angles(self, tmp_path: Path) -> None:
        from tomviz.io.formats.dicom import DicomReader, DicomWriter

        arr = np.zeros((3, 3, 3), dtype=np.uint16)
        img = _make_image_data(arr)

        path = str(tmp_path / 'test.dcm')
        DicomWriter().write(path, img)

        result = DicomReader().read(path)
        result_angles = _vtk_get_tilt_angles(result)
        assert result_angles is None


# ---------------------------------------------------------------------------
# ZEISS Xradia reader
# ---------------------------------------------------------------------------

try:
    import olefile
    _HAS_OLEFILE = True
except ImportError:
    _HAS_OLEFILE = False


class _FakeStream:

    def __init__(self, data: bytes) -> None:
        self._data = data

    def read(self) -> bytes:
        return self._data


class _FakeOleFile:
    """Stands in for olefile.OleFileIO, which cannot write OLE files."""

    streams: dict[str, bytes] = {}

    def __init__(self, path: str) -> None:
        pass

    def __enter__(self) -> _FakeOleFile:
        return self

    def __exit__(self, *args: object) -> None:
        pass

    def exists(self, name: str) -> bool:
        return name in self.streams

    def openstream(self, name: str) -> _FakeStream:
        return _FakeStream(self.streams[name])


def _zeiss_streams(
    frames: npt.NDArray[np.generic],
    data_type: int,
    mode: int,
    angles: npt.NDArray[np.float32] | None = None,
    pixel_size: float | None = 2.5,
) -> dict[str, bytes]:
    n, height, width = frames.shape
    streams = {
        'ImageInfo/NoOfImages': np.uint32(n).tobytes(),
        'ImageInfo/ImageWidth': np.uint32(width).tobytes(),
        'ImageInfo/ImageHeight': np.uint32(height).tobytes(),
        'ImageInfo/DataType': np.uint32(data_type).tobytes(),
        'ImageInfo/AcquisitionMode': np.uint32(mode).tobytes(),
    }
    if pixel_size is not None:
        streams['ImageInfo/PixelSize'] = np.float32(pixel_size).tobytes()
    if angles is not None:
        streams['ImageInfo/Angles'] = angles.astype('<f4').tobytes()
    for i, frame in enumerate(frames):
        streams[f'ImageData{i // 100 + 1}/Image{i + 1}'] = frame.tobytes()
    return streams


def _read_zeiss(streams: dict[str, bytes], name: str) -> vtkImageData:
    from tomviz.io.formats.zeiss import ZeissReader

    with mock.patch.object(_FakeOleFile, 'streams', streams), \
            mock.patch('olefile.OleFileIO', _FakeOleFile):
        return ZeissReader().read(name)


def _units(image_data: vtkImageData) -> list[str]:
    units = image_data.GetFieldData().GetAbstractArray('units')
    return [units.GetValue(i) for i in range(units.GetNumberOfValues())]


@pytest.mark.skipif(not _HAS_OLEFILE, reason='olefile not available')
class TestZeiss:

    def test_zeiss_txrm_reads_tilt_series(self) -> None:
        # 101 projections, so the last one is in the second storage
        # (ImageData2/Image101).
        frames = np.arange(101 * 3 * 4, dtype=np.uint16).reshape((101, 3, 4))
        angles = np.linspace(-90.0, 90.0, 101, dtype=np.float32)
        result = _read_zeiss(_zeiss_streams(frames, 5, 0, angles), 'a.txrm')

        # x is the image row, y the image column, z the projection.
        result_arr = _vtk_get_array(result)
        assert result_arr.dtype == np.uint16
        np.testing.assert_array_equal(result_arr,
                                      np.transpose(frames, (1, 2, 0)))

        result_angles = _vtk_get_tilt_angles(result)
        assert result_angles is not None
        np.testing.assert_allclose(result_angles, angles)

        assert result.GetSpacing() == pytest.approx((2.5, 2.5, 2.5))
        assert _units(result) == ['um', 'um', 'um']

    def test_zeiss_txm_reads_volume(self) -> None:
        frames = np.random.default_rng(9).random((5, 3, 4)).astype(np.float32)
        angles = np.zeros(5, dtype=np.float32)
        result = _read_zeiss(_zeiss_streams(frames, 10, 10, angles), 'a.txm')

        # x is the image column, y the image row reversed (as VTK reads
        # ZEISS's TIFF export), z the slice.
        result_arr = _vtk_get_array(result)
        assert result_arr.dtype == np.float32
        np.testing.assert_array_equal(
            result_arr, np.transpose(frames[:, ::-1, :], (2, 1, 0)))

        assert _vtk_get_tilt_angles(result) is None
        assert _units(result) == ['um', 'um', 'um']

    def test_zeiss_single_image_is_not_a_tilt_series(self) -> None:
        frames = np.arange(12, dtype=np.uint16).reshape((1, 3, 4))
        angles = np.zeros(1, dtype=np.float32)
        result = _read_zeiss(_zeiss_streams(frames, 5, 0, angles), 'a.xrm')

        assert result.GetDimensions() == (4, 3, 1)
        assert _vtk_get_tilt_angles(result) is None

    def test_zeiss_projections_without_all_angles_read_as_volume(
        self, capsys: pytest.CaptureFixture[str],
    ) -> None:
        frames = np.arange(3 * 3 * 4, dtype=np.uint16).reshape((3, 3, 4))
        angles = np.array([0.0, 1.0], dtype=np.float32)
        result = _read_zeiss(_zeiss_streams(frames, 5, 0, angles), 'a.txrm')

        assert result.GetDimensions() == (4, 3, 3)
        assert _vtk_get_tilt_angles(result) is None
        assert '3 projections but 2 angles' in capsys.readouterr().out

    @pytest.mark.parametrize('pixel_size', [None, 0.0, float('nan')])
    def test_zeiss_without_pixel_size_has_no_units(
        self, pixel_size: float | None,
    ) -> None:
        frames = np.zeros((1, 3, 4), dtype=np.uint16)
        streams = _zeiss_streams(frames, 5, 2, pixel_size=pixel_size)
        result = _read_zeiss(streams, 'a.xrm')

        assert result.GetSpacing() == (1.0, 1.0, 1.0)
        assert result.GetFieldData().GetAbstractArray('units') is None

    def test_zeiss_unsupported_data_type_reads_nothing(self) -> None:
        frames = np.zeros((2, 3, 4), dtype=np.uint8)
        angles = np.zeros(2, dtype=np.float32)
        result = _read_zeiss(_zeiss_streams(frames, 3, 0, angles), 'a.txrm')

        assert result.GetNumberOfPoints() == 0

# ---------------------------------------------------------------------------
# Amira / Avizo reader
# ---------------------------------------------------------------------------

def _rle_encode(data: bytes) -> bytes:
    """HxByteRLE: a count byte n, then n literal bytes if n > 127 (minus
    128), else one byte repeated n times."""
    out = bytearray()
    literal = bytearray()

    def flush() -> None:
        if literal:
            out.extend(bytes([128 + len(literal)]) + literal)
            literal.clear()

    i = 0
    while i < len(data):
        n = 1
        while i + n < len(data) and data[i + n] == data[i] and n < 127:
            n += 1
        if n > 2:
            flush()
            out.extend(bytes([n, data[i]]))
        else:
            literal.extend(data[i:i + n])
            if len(literal) >= 120:
                flush()
        i += n
    flush()
    return bytes(out)


def _write_amira(
    path: Path,
    data: npt.NDArray[np.generic],
    kind: str,
    mode: str = 'BINARY-LITTLE-ENDIAN',
    encoding: str | None = None,
    bounding_box: str = '0 3 0 4 0 5',
    coord_type: str = 'uniform',
    first_line: str = '# AmiraMesh',
) -> Path:
    """An AmiraMesh file of the (x, y, z) array, written x fastest."""
    nx, ny, nz = data.shape
    values = np.transpose(data, (2, 1, 0)).ravel()
    if mode == 'ASCII':
        body = ' '.join(str(v) for v in values).encode() + b'\n'
        section = '@1'
    else:
        order = '<' if mode == 'BINARY-LITTLE-ENDIAN' else '>'
        body = values.astype(values.dtype.newbyteorder(order)).tobytes()
        section = '@1'
        if encoding == 'HxZip':
            body = zlib.compress(body)
        elif encoding == 'HxByteRLE':
            body = _rle_encode(body)
        if encoding:
            section = f'@1({encoding},{len(body)})'
    header = (f'{first_line} {mode} 2.1\n\n'
              f'define Lattice {nx} {ny} {nz}\n\n'
              'Parameters {\n'
              f'    BoundingBox {bounding_box},\n'
              f'    CoordType "{coord_type}"\n'
              '}\n\n'
              f'Lattice {{ {kind} Data }} {section}\n\n'
              '# Data section follows\n@1\n')
    path.write_bytes(header.encode() + body)
    return path


@pytest.mark.parametrize('mode,encoding', [
    ('BINARY-LITTLE-ENDIAN', None),
    ('BINARY', None),
    ('BINARY-LITTLE-ENDIAN', 'HxZip'),
    ('BINARY-LITTLE-ENDIAN', 'HxByteRLE'),
    ('ASCII', None),
])
def test_amira_reads_uniform_lattice(tmp_path: Path, mode: str,
                                     encoding: str | None) -> None:
    from tomviz.io.formats.amira import AmiraReader

    # A few long runs and some noise, so RLE has repeats and literals.
    arr = np.zeros((4, 5, 6), dtype=np.uint16)
    arr[1:3, 2:4, :] = np.arange(12, dtype=np.uint16).reshape((2, 2, 3)) \
        .repeat(2, axis=2) * 300
    path = _write_amira(tmp_path / 'a.am', arr, 'ushort', mode, encoding)
    result = AmiraReader().read(str(path))

    result_arr = _vtk_get_array(result)
    assert result_arr.dtype == np.uint16
    np.testing.assert_array_equal(result_arr, arr)
    # The bounding box spans the voxel centres.
    assert result.GetOrigin() == pytest.approx((0.0, 0.0, 0.0))
    assert result.GetSpacing() == pytest.approx((1.0, 1.0, 1.0))


def test_amira_float_volume_with_offset_box(tmp_path: Path) -> None:
    from tomviz.io.formats.amira import AmiraReader

    arr = np.random.default_rng(3).random((3, 4, 2)).astype(np.float32)
    path = _write_amira(tmp_path / 'a.am', arr, 'float',
                        bounding_box='-1 0 0 1.5 10 12',
                        first_line='# Avizo')
    result = AmiraReader().read(str(path))

    np.testing.assert_array_equal(_vtk_get_array(result), arr)
    assert result.GetOrigin() == pytest.approx((-1.0, 0.0, 10.0))
    assert result.GetSpacing() == pytest.approx((0.5, 0.5, 2.0))


def test_amira_single_slice_takes_box_size_as_spacing(
        tmp_path: Path) -> None:
    from tomviz.io.formats.amira import AmiraReader

    arr = np.ones((2, 2, 1), dtype=np.uint8)
    path = _write_amira(tmp_path / 'a.am', arr, 'byte',
                        bounding_box='0 0.5 0 0.5 0 2')
    result = AmiraReader().read(str(path))
    assert result.GetSpacing() == pytest.approx((0.5, 0.5, 2.0))


@pytest.mark.parametrize('kind,coord_type,message', [
    ('float[3]', 'uniform', 'not a scalar field'),
    ('float', 'rectilinear', 'not uniform'),
])
def test_amira_refuses_what_tomviz_cannot_show(
        tmp_path: Path, capsys: pytest.CaptureFixture[str], kind: str,
        coord_type: str, message: str) -> None:
    from tomviz.io.formats.amira import AmiraReader

    arr = np.zeros((2, 2, 2), dtype=np.float32)
    path = _write_amira(tmp_path / 'a.am', arr, kind, coord_type=coord_type)
    assert AmiraReader().read(str(path)).GetNumberOfPoints() == 0
    assert message in capsys.readouterr().out


@pytest.mark.parametrize('encoding', [None, 'HxZip', 'HxByteRLE'])
def test_amira_refuses_truncated_data(
        tmp_path: Path, capsys: pytest.CaptureFixture[str],
        encoding: str | None) -> None:
    from tomviz.io.formats.amira import AmiraReader

    arr = np.arange(64, dtype=np.uint8).reshape((4, 4, 4))
    path = _write_amira(tmp_path / 'a.am', arr, 'byte', encoding=encoding)
    path.write_bytes(path.read_bytes()[:-10])
    assert AmiraReader().read(str(path)).GetNumberOfPoints() == 0
    assert 'damaged or shorter' in capsys.readouterr().out


def test_amira_header_as_nat_writes_it(tmp_path: Path) -> None:
    # natverse's writer puts "=" before @1 and no "# Data section follows";
    # extra spaces are allowed, and a 2D lattice is one slice.
    from tomviz.io.formats.amira import AmiraReader

    path = tmp_path / 'a.am'
    path.write_bytes(
        b'# AmiraMesh BINARY-LITTLE-ENDIAN 2.1\n'
        b'define Lattice  3   2\n'
        b'Parameters {\n'
        b'    Sub {\n        BoundingBox 9 9 9 9 9 9\n    }\n'
        b'    BoundingBox 0 2 0 1 0 0,\n'
        b'    CoordType "uniform"\n}\n'
        b'Lattice { float ScalarField } = @1\n'
        b'@1\n' + np.arange(6, dtype='<f4').tobytes())
    result = AmiraReader().read(str(path))

    assert result.GetDimensions() == (3, 2, 1)
    np.testing.assert_array_equal(_vtk_get_array(result)[:, :, 0],
                                  [[0, 3], [1, 4], [2, 5]])
    # The top-level BoundingBox, not the nested one.
    assert result.GetOrigin() == pytest.approx((0.0, 0.0, 0.0))
    assert result.GetSpacing() == pytest.approx((1.0, 1.0, 1.0))


def test_amira_byte_rle_by_hand(tmp_path: Path) -> None:
    # Bytes 5 5 5 200 201 202 0 0 as HxByteRLE: 3 x 5, then 3 literal
    # bytes (0x80 + 3), then 2 x 0. Byte data is unsigned.
    from tomviz.io.formats.amira import AmiraReader

    rle = bytes([3, 5, 0x83, 200, 201, 202, 2, 0])
    path = tmp_path / 'a.am'
    path.write_bytes(
        b'# AmiraMesh 3D BINARY 2.0\n'
        b'define Lattice 2 2 2\n'
        b'Parameters {\n    CoordType "uniform"\n}\n'
        b'Lattice { byte Labels } @1(HxByteRLE,8)\n'
        b'# Data section follows\n'
        b'@1\n' + rle)
    result_arr = _vtk_get_array(AmiraReader().read(str(path)))

    assert result_arr.dtype == np.uint8
    np.testing.assert_array_equal(
        np.transpose(result_arr, (2, 1, 0)).ravel(),
        [5, 5, 5, 200, 201, 202, 0, 0])


def test_amira_long_volume(tmp_path: Path) -> None:
    from tomviz.io.formats.amira import AmiraReader

    # Big endian 64-bit integers. (This file's VTK helper stores them as
    # float32, so the values stay below 2**24.)
    arr = (np.arange(24, dtype=np.int64) * 1000 - 5000).reshape((2, 3, 4))
    path = _write_amira(tmp_path / 'a.am', arr, 'long', 'BINARY')
    result_arr = _vtk_get_array(AmiraReader().read(str(path)))
    np.testing.assert_array_equal(result_arr, arr)
