import numpy as np

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)

from utils import OPERATOR_PATH


def _run_rotate(arr, **arguments):
    """Run the real Rotate operator from tomviz/python/ over `arr`."""
    t = LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / 'Rotate3D.json').read_text(),
        'script': (OPERATOR_PATH / 'Rotate3D.py').read_text(),
        'arguments': arguments,
    })
    ds = Dataset({'ImageScalars': arr}, 'ImageScalars')
    ds.spacing = (1.0, 1.0, 1.0)
    result = t.transform({'volume': PortData(ds, 'ImageData')})
    return result[t.output_ports()[0].name].payload.active_scalars


def _slab_with_marker():
    # tomviz arrays are (nx, ny, nz). Put a marker near the top in y.
    arr = np.zeros((64, 64, 20), dtype=np.float32, order='F')
    arr[8:56, 8:56, 4:16] = 1.0
    arr[28:36, 48:54, 8:12] = 5.0
    return arr


def _marker_center(arr):
    return np.argwhere(arr > 3.0).mean(axis=0)


def test_rotate_in_plane_without_expand_keeps_shape():
    """An alignment rotation about Z should turn the data within the image
    plane, leave the dimensions alone, and leave the marker at the top."""
    arr = _slab_with_marker()
    before = _marker_center(arr)
    out = _run_rotate(arr, rotation_angle=3.0, rotation_axis=2, expand=False)

    assert out.shape == arr.shape
    after = _marker_center(out)
    # Still near the top in y, and still in the same slices.
    assert after[1] > arr.shape[1] * 0.6
    assert abs(after[2] - before[2]) < 1.0


def test_rotate_expand_pads_the_perpendicular_axis():
    """Expanding grows the box to hold the rotated volume and zero-pads the
    corners, so the leading slices of the grown axis are nearly empty. This
    is why a small rotation can appear to blank the start of a dataset."""
    arr = _slab_with_marker()
    out = _run_rotate(arr, rotation_angle=3.0, rotation_axis=0, expand=True)

    # Rotating about x tips y into z, so z grows by about ny * sin(3 deg).
    assert out.shape[2] > arr.shape[2]
    filled = [(out[:, :, k] > 0.01).mean() for k in range(out.shape[2])]
    assert filled[0] < 0.5 * filled[out.shape[2] // 2]

    # Turning expansion off is what avoids that.
    kept = _run_rotate(arr, rotation_angle=3.0, rotation_axis=0, expand=False)
    assert kept.shape == arr.shape
