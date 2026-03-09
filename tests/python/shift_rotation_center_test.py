import numpy as np
import os

import pytest

from utils import load_operator_module

from tomviz.external_dataset import Dataset

try:
    import tomopy
    HAS_TOMOPY = True
except ImportError:
    HAS_TOMOPY = False


def _make_synthetic_dataset(shape=(20, 64, 30), num_arrays=1, spacing=None):
    """Create a synthetic dataset with a bright column at the center of axis 1.

    This makes shift effects easy to detect: the column moves along axis 1.
    """
    arrays = {}
    for i in range(num_arrays):
        arr = np.zeros(shape, dtype=np.float32)
        mid_y = shape[1] // 2
        # Place a bright column at the center of Y
        arr[:, mid_y - 1:mid_y + 2, :] = 1.0 + i
        name = f'array_{i}' if num_arrays > 1 else 'intensity'
        arrays[name] = arr

    dataset = Dataset(arrays)
    if spacing is not None:
        dataset.spacing = spacing
    else:
        dataset.spacing = [1.0, 1.0, 1.0]
    dataset.tilt_angles = np.linspace(-70, 70, shape[2])
    return dataset


def test_shift_rotation_center_manual():
    """Test that a manual pixel shift moves data along axis 1."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    dataset = _make_synthetic_dataset()
    original = dataset.active_scalars.copy()

    # A positive rotation_center means shift left (negative direction)
    shift_px = 5
    module.transform(dataset, rotation_center=shift_px)

    result = dataset.active_scalars

    # Shape must be preserved
    assert result.shape == original.shape

    # The data should have changed
    assert not np.allclose(result, original)

    # The bright column was at center. After shifting by -5 pixels along
    # axis 1, the column should move. Check that the peak position shifted.
    orig_peak_y = np.argmax(original[0, :, 0])
    new_peak_y = np.argmax(result[0, :, 0])
    assert new_peak_y < orig_peak_y, "Peak should have moved left (negative)"
    assert abs((orig_peak_y - new_peak_y) - shift_px) <= 1


def test_shift_rotation_center_zero():
    """Test that a zero shift leaves the data unchanged."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    dataset = _make_synthetic_dataset()
    original = dataset.active_scalars.copy()

    module.transform(dataset, rotation_center=0)

    assert np.allclose(dataset.active_scalars, original)


def test_shift_rotation_center_multi_array():
    """Test that the shift is applied to all scalar arrays."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    dataset = _make_synthetic_dataset(num_arrays=3)
    originals = {name: arr.copy() for name, arr in dataset.arrays.items()}

    shift_px = 3
    module.transform(dataset, rotation_center=shift_px)

    # All arrays should have been modified
    for name in dataset.scalars_names:
        assert not np.allclose(dataset.arrays[name], originals[name]), \
            f"Array '{name}' was not shifted"

    # All arrays should have the same shift pattern (peak moved by same amount)
    shifts = []
    for name in dataset.scalars_names:
        orig_peak = np.argmax(originals[name][0, :, 0])
        new_peak = np.argmax(dataset.arrays[name][0, :, 0])
        shifts.append(orig_peak - new_peak)

    assert all(s == shifts[0] for s in shifts), \
        "All arrays should be shifted by the same amount"


def test_shift_rotation_center_npz_save(tmp_path):
    """Test that NPZ file is saved with correct keys and values."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    dataset = _make_synthetic_dataset(spacing=[1.0, 2.5, 1.0])
    save_path = str(tmp_path / 'transforms.npz')

    shift_px = 7
    module.transform(
        dataset,
        rotation_center=shift_px,
        transform_source='manual',
        transforms_save_file=save_path,
    )

    assert os.path.exists(save_path)

    with np.load(save_path) as f:
        assert 'rotation_center' in f
        assert 'spacing' in f
        assert float(f['rotation_center']) == shift_px
        assert np.allclose(f['spacing'], [1.0, 2.5, 1.0])


def test_shift_rotation_center_npz_load(tmp_path):
    """Test loading a shift from an NPZ file, including spacing scaling."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    # Save with spacing [1.0, 2.0, 1.0] and a 10-pixel shift
    save_path = str(tmp_path / 'transforms.npz')
    np.savez(save_path, rotation_center=10, spacing=[1.0, 2.0, 1.0])

    # Load onto a dataset with spacing [1.0, 1.0, 1.0]
    # Expected scaled shift: 10 * 2.0 / 1.0 = 20 pixels
    dataset = _make_synthetic_dataset(
        shape=(20, 80, 30),
        spacing=[1.0, 1.0, 1.0],
    )
    original = dataset.active_scalars.copy()

    module.transform(
        dataset,
        transform_source='from_file',
        transform_file=save_path,
    )

    result = dataset.active_scalars
    assert not np.allclose(result, original)

    # Verify the shift magnitude is approximately 20 pixels
    orig_peak = np.argmax(original[0, :, 0])
    new_peak = np.argmax(result[0, :, 0])
    actual_shift = orig_peak - new_peak
    assert abs(actual_shift - 20) <= 1, \
        f"Expected ~20px shift from spacing scaling, got {actual_shift}"


def test_shift_rotation_center_npz_roundtrip(tmp_path):
    """Test save then load produces equivalent results."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    spacing = [1.0, 1.5, 1.0]
    shift_px = 4

    # Apply manually and save
    dataset_manual = _make_synthetic_dataset(spacing=spacing)
    save_path = str(tmp_path / 'transforms.npz')
    module.transform(
        dataset_manual,
        rotation_center=shift_px,
        transform_source='manual',
        transforms_save_file=save_path,
    )

    # Load from file on a fresh dataset with same spacing
    dataset_loaded = _make_synthetic_dataset(spacing=spacing)
    module.transform(
        dataset_loaded,
        transform_source='from_file',
        transform_file=save_path,
    )

    assert np.allclose(
        dataset_manual.active_scalars,
        dataset_loaded.active_scalars,
    ), "Round-trip save/load should produce identical results"


def test_shift_no_save_when_loading(tmp_path):
    """Test that loading from file does not write a save file."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    # Create an NPZ to load from
    load_path = str(tmp_path / 'input.npz')
    np.savez(load_path, rotation_center=3, spacing=[1.0, 1.0, 1.0])

    # Provide a save path, but since transform_source='from_file',
    # the save should NOT happen
    save_path = str(tmp_path / 'should_not_exist.npz')
    dataset = _make_synthetic_dataset()
    module.transform(
        dataset,
        transform_source='from_file',
        transform_file=load_path,
        transforms_save_file=save_path,
    )

    assert not os.path.exists(save_path)


# --- QiA and QN quality metric tests ---

def test_qia_metric():
    """Test that Qia returns list of scores and best index."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    # Create synthetic reconstruction images
    # One with all positive values, one with mixed values
    images = np.zeros((3, 16, 16), dtype=np.float32)
    images[0] = np.abs(np.random.rand(16, 16)) + 0.1  # All positive
    images[1] = np.random.rand(16, 16) - 0.5           # Mixed
    images[2] = np.abs(np.random.rand(16, 16)) + 0.5   # Larger positive

    qlist, best_idx = module.Qia(images)

    assert isinstance(qlist, list)
    assert len(qlist) == 3
    assert isinstance(best_idx, int)
    assert 0 <= best_idx < 3


def test_qn_metric():
    """Test that Qn returns list of scores and best index."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    images = np.zeros((3, 16, 16), dtype=np.float32)
    images[0] = np.random.rand(16, 16) - 0.3  # Some negative values
    images[1] = np.random.rand(16, 16) - 0.5  # More negative values
    images[2] = np.abs(np.random.rand(16, 16))  # All positive

    qlist, best_idx = module.Qn(images)

    assert isinstance(qlist, list)
    assert len(qlist) == 3
    assert isinstance(best_idx, int)
    assert 0 <= best_idx < 3


def test_qia_uniform_gives_equal_scores():
    """Uniform images should have identical QiA scores."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    images = np.ones((5, 16, 16), dtype=np.float32) * 3.0

    qlist, _ = module.Qia(images)

    # All scores should be equal for identical images
    for i in range(1, len(qlist)):
        assert abs(qlist[i] - qlist[0]) < 1e-6, \
            f"Score {i} ({qlist[i]}) differs from score 0 ({qlist[0]})"


def test_qn_all_positive_gives_zeros():
    """All-positive reconstruction should give near-zero QN values."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    images = np.abs(np.random.rand(5, 16, 16).astype(np.float32)) + 0.1

    qlist, _ = module.Qn(images)

    # With all positive values, the negativity metric should be ~0
    for val in qlist:
        assert abs(val) < 1e-6, \
            f"QN value {val} should be near zero for all-positive images"


@pytest.mark.skipif(not HAS_TOMOPY, reason="tomopy not installed")
def test_rotations_basic():
    """Test test_rotations returns correct structure."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    # Create a synthetic tilt series
    num_angles = 20
    detector_width = 32
    num_slices = 1

    arrays = {'intensity': np.random.rand(
        num_angles, detector_width, num_slices
    ).astype(np.float32) + 0.1}

    dataset = Dataset(arrays)
    dataset.spacing = [1.0, 1.0, 1.0]
    dataset.tilt_angles = np.linspace(-70, 70, num_angles)
    dataset.tilt_axis = 0

    steps = 5
    result = module.test_rotations(
        dataset,
        start=-3,
        stop=3,
        steps=steps,
        sli=0,
        algorithm='gridrec',
    )

    assert 'images' in result
    assert 'centers' in result
    assert 'qia' in result
    assert 'qn' in result

    # Verify centers length matches steps
    assert len(result['centers']) == steps

    # Verify QiA and QN are lists with correct length
    assert len(result['qia']) == steps
    assert len(result['qn']) == steps
