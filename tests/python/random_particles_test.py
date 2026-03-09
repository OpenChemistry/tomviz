import warnings

import numpy as np
import pytest

from utils import load_operator_module


def test_generate_dataset_basic():
    """Test that generate_dataset produces non-zero output."""
    module = load_operator_module('RandomParticles')

    array = np.zeros((16, 16, 16), dtype=np.float64)
    module.generate_dataset(array)

    assert not np.allclose(array, 0), "Output should not be all zeros"
    assert np.amax(array) == pytest.approx(1.0, abs=0.01), \
        "Max value should be approximately 1.0 after normalization"


def test_generate_dataset_shape_preserved():
    """Test that output shape matches input shape."""
    module = load_operator_module('RandomParticles')

    shape = (20, 24, 18)
    array = np.zeros(shape, dtype=np.float64)
    module.generate_dataset(array)

    assert array.shape == shape


def test_generate_dataset_sparsity():
    """Test that sparsity parameter controls fraction of zero voxels."""
    module = load_operator_module('RandomParticles')

    array = np.zeros((32, 32, 32), dtype=np.float64)
    sparsity = 0.1
    module.generate_dataset(array, sparsity=sparsity)

    zero_fraction = np.count_nonzero(array == 0) / array.size
    # With sparsity=0.1, approximately 90% of voxels should be zero
    assert zero_fraction > 0.8, \
        f"Expected ~90% zeros with sparsity=0.1, got {zero_fraction*100:.1f}%"


def test_generate_dataset_uses_int64():
    """Test that no numpy deprecation warnings are raised (the fix changed
    np.int to np.int64)."""
    module = load_operator_module('RandomParticles')

    array = np.zeros((16, 16, 16), dtype=np.float64)
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always", DeprecationWarning)
        module.generate_dataset(array)

    numpy_deprecation_warnings = [
        x for x in w
        if issubclass(x.category, DeprecationWarning) and 'numpy' in str(x.message).lower()
    ]
    assert len(numpy_deprecation_warnings) == 0, \
        f"Got numpy deprecation warnings: {[str(x.message) for x in numpy_deprecation_warnings]}"
