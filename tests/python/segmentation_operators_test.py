import numpy as np
import pytest

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)

from utils import OPERATOR_PATH


def _run(name, arr, port_type='ImageData', **arguments):
    t = LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / f'{name}.json').read_text(),
        'script': (OPERATOR_PATH / f'{name}.py').read_text(),
        'arguments': arguments,
    })
    ds = Dataset({'ImageScalars': np.asfortranarray(arr)}, 'ImageScalars')
    ds.spacing = (1.0, 1.0, 1.0)
    result = t.transform({'volume': PortData(ds, port_type)})
    return result[t.output_ports()[0].name].payload.active_scalars


def _threshold(arr, **arguments):
    return _run('BinaryThreshold', arr, **arguments)


def _components(arr, **arguments):
    return _run('ConnectedComponents', arr, 'LabelMap', **arguments)


# --- Binary Threshold ---

def test_threshold_keeps_the_closed_interval_as_uint8():
    arr = np.arange(27, dtype=np.float32).reshape(3, 3, 3)
    out = _threshold(arr, lower_threshold=5.0, upper_threshold=9.0)
    assert out.dtype == np.uint8
    assert out.shape == arr.shape
    expected = ((arr >= 5) & (arr <= 9)).astype(np.uint8)
    assert np.array_equal(out, expected)


def test_threshold_rounds_fractional_thresholds_inward_for_integers():
    arr = np.arange(27, dtype=np.int16).reshape(3, 3, 3)
    out = _threshold(arr, lower_threshold=4.2, upper_threshold=8.9)
    # 4.2 rounds up to 5 and 8.9 rounds down to 8
    expected = ((arr >= 5) & (arr <= 8)).astype(np.uint8)
    assert np.array_equal(out, expected)


def test_threshold_refuses_an_empty_integer_interval():
    arr = np.arange(27, dtype=np.uint8).reshape(3, 3, 3)
    with pytest.raises(Exception):
        _threshold(arr, lower_threshold=4.2, upper_threshold=4.8)


def test_threshold_sends_nan_to_the_background():
    arr = np.ones((2, 2, 2), dtype=np.float32)
    arr[0, 0, 0] = np.nan
    out = _threshold(arr, lower_threshold=0.0, upper_threshold=2.0)
    assert out[0, 0, 0] == 0
    assert out.sum() == 7


def test_threshold_matches_itk_when_available():
    itk = pytest.importorskip('itk')
    rng = np.random.default_rng(1)
    arr = (rng.random((12, 11, 10)) * 255).astype(np.float32)
    out = _threshold(arr, lower_threshold=40.0, upper_threshold=200.0)

    image = itk.GetImageViewFromArray(np.ascontiguousarray(arr))
    f = itk.BinaryThresholdImageFilter[type(image), itk.Image.UC3].New()
    f.SetLowerThreshold(40.0)
    f.SetUpperThreshold(200.0)
    f.SetInsideValue(1)
    f.SetOutsideValue(0)
    f.SetInput(image)
    f.Update()
    reference = itk.GetArrayFromImage(f.GetOutput())
    assert np.array_equal(out, reference)


# --- Connected Components ---

def _two_blobs():
    arr = np.zeros((10, 10, 10), dtype=np.uint8)
    arr[1:3, 1:3, 1:3] = 1   # 8 voxels
    arr[5:9, 5:9, 5:9] = 1   # 64 voxels
    return arr


def test_components_number_by_size_smallest_first():
    out = _components(_two_blobs())
    assert out.dtype == np.uint8
    assert out.max() == 2
    assert np.all(out[1:3, 1:3, 1:3] == 1)
    assert np.all(out[5:9, 5:9, 5:9] == 2)
    assert out.sum() == 8 * 1 + 64 * 2


def test_components_treat_every_non_background_value_as_foreground():
    arr = _two_blobs()
    arr[1:3, 1:3, 1:3] = 7
    out = _components(arr)
    assert out.max() == 2
    # A different background value flips what is foreground
    out = _components(arr, background_value=7)
    assert out.max() == 1
    assert out[1, 1, 1] == 0 and out[0, 0, 0] == 1


def test_components_connectivity_decides_whether_corners_touch():
    arr = np.zeros((4, 4, 4), dtype=np.uint8)
    arr[1, 1, 1] = 1
    arr[2, 2, 2] = 1  # touches the first only at a corner
    assert _components(arr, connectivity=1).max() == 2
    assert _components(arr, connectivity=2).max() == 2
    assert _components(arr, connectivity=3).max() == 1
    arr[2, 2, 1] = 1  # bridges the two along an edge
    assert _components(arr, connectivity=1).max() == 2
    assert _components(arr, connectivity=2).max() == 1


def test_components_drop_specks_below_the_minimum_size():
    arr = _two_blobs()
    arr[0, 9, 9] = 1  # a single stray voxel
    out = _components(arr)
    assert out.max() == 3 and out[0, 9, 9] == 1
    out = _components(arr, min_size=2)
    assert out.max() == 2 and out[0, 9, 9] == 0
    out = _components(arr, min_size=9)
    assert out.max() == 1 and np.all(out[1:3, 1:3, 1:3] == 0)
    assert np.all(out[5:9, 5:9, 5:9] == 1)


def test_components_widen_the_label_type_when_needed():
    # A lattice of isolated voxels: 5 * 6 * 10 = 300 components
    arr = np.zeros((10, 12, 20), dtype=np.uint8)
    arr[::2, ::2, ::2] = 1
    out = _components(arr)
    assert out.max() == 300
    assert out.dtype == np.uint16


def test_components_accept_a_float_mask():
    arr = _two_blobs().astype(np.float32)
    out = _components(arr)
    assert out.max() == 2


def test_components_of_an_empty_volume_are_all_background():
    out = _components(np.zeros((3, 3, 3), dtype=np.uint8))
    assert out.dtype == np.uint8
    assert not out.any()


def test_components_partition_matches_itk_when_available():
    itk = pytest.importorskip('itk')
    rng = np.random.default_rng(2)
    arr = (rng.random((16, 15, 14)) < 0.3).astype(np.uint8)
    out = _components(arr)

    image = itk.GetImageViewFromArray(np.ascontiguousarray(
        arr.astype(np.uint16)))
    T = type(image)
    f = itk.ConnectedComponentImageFilter[T, T].New()
    f.SetInput(image)
    f.SetBackgroundValue(0)
    r = itk.RelabelComponentImageFilter[T, T].New()
    r.SetInput(f.GetOutput())
    r.SortByObjectSizeOn()
    r.Update()
    reference = itk.GetArrayFromImage(r.GetOutput())

    # Same foreground and the same number of components
    assert np.array_equal(out != 0, reference != 0)
    assert out.max() == reference.max()
    # Every component is the same set of voxels in both labelings
    ours = {frozenset(np.flatnonzero(out == label))
            for label in range(1, out.max() + 1)}
    theirs = {frozenset(np.flatnonzero(reference == label))
              for label in range(1, reference.max() + 1)}
    assert ours == theirs
    # Ordering by size, largest label = largest component
    sizes = np.bincount(out.ravel())[1:]
    assert np.all(np.diff(sizes) >= 0)
