import numpy as np
import pytest

from utils import load_operator_module

from tomviz.external_dataset import Dataset


def _make_dataset(data):
    """Create a Dataset wrapping the given numpy array."""
    arrays = {'scalars': np.array(data, dtype=np.float32)}
    ds = Dataset(arrays)
    ds.spacing = [1.0, 1.0, 1.0]
    return ds


def test_normalize_basic():
    """Test that normalization equalizes total intensity across slices."""
    module = load_operator_module('NormalizeTiltSeries')

    # Create tilt series with varying intensities per slice (axis 2)
    shape = (10, 10, 5)
    data = np.ones(shape, dtype=np.float32)
    for i in range(shape[2]):
        data[:, :, i] *= (i + 1)  # Slice intensities: 1, 2, 3, 4, 5

    dataset = _make_dataset(data)
    module.transform(dataset)

    result = dataset.active_scalars
    assert result.shape == shape

    # After normalization, all slices should have approximately equal
    # total intensity
    slice_sums = [np.sum(result[:, :, i]) for i in range(shape[2])]
    mean_sum = np.mean(slice_sums)
    for i, s in enumerate(slice_sums):
        assert abs(s - mean_sum) / mean_sum < 0.01, \
            f"Slice {i} sum {s} differs from mean {mean_sum}"


def test_normalize_preserves_shape():
    """Test that output shape matches input shape."""
    module = load_operator_module('NormalizeTiltSeries')

    shape = (8, 12, 6)
    data = np.random.rand(*shape).astype(np.float32) + 0.1
    dataset = _make_dataset(data)
    module.transform(dataset)

    assert dataset.active_scalars.shape == shape


def test_normalize_zero_slice_no_error():
    """Test that normalization handles all-zero slices without crashing
    (the divide-by-zero fix)."""
    module = load_operator_module('NormalizeTiltSeries')

    shape = (10, 10, 5)
    data = np.ones(shape, dtype=np.float32)
    data[:, :, 2] = 0.0  # All-zero slice

    dataset = _make_dataset(data)

    # Should not raise an exception
    module.transform(dataset)

    result = dataset.active_scalars
    assert result.shape == shape

    # The zero slice should remain zero (no NaN/Inf from division)
    assert np.allclose(result[:, :, 2], 0.0)
    assert not np.any(np.isnan(result))
    assert not np.any(np.isinf(result))


def test_normalize_already_uniform():
    """Test that uniform data remains approximately unchanged."""
    module = load_operator_module('NormalizeTiltSeries')

    shape = (10, 10, 5)
    data = np.ones(shape, dtype=np.float32) * 3.0
    dataset = _make_dataset(data)

    original = dataset.active_scalars.copy()
    module.transform(dataset)

    result = dataset.active_scalars
    assert np.allclose(result, original, rtol=1e-5)
