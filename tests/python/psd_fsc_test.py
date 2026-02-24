import numpy as np
import pytest

from utils import load_operator_module


# --- PSD tests ---

def _load_psd_functions():
    module = load_operator_module('PowerSpectrumDensity')
    return module.pad_to_cubic, module.psd3D


def test_pad_to_cubic():
    """Verify non-cubic array is padded to cubic shape."""
    pad_to_cubic, _ = _load_psd_functions()

    arr = np.random.rand(8, 12, 16)
    result = pad_to_cubic(arr)

    assert result.shape == (16, 16, 16)
    # Original data should be preserved in the unpadded region
    assert np.allclose(result[:8, :12, :16], arr)


def test_pad_to_cubic_already_cubic():
    """Cubic array should be unchanged in shape."""
    pad_to_cubic, _ = _load_psd_functions()

    arr = np.random.rand(10, 10, 10)
    result = pad_to_cubic(arr)

    assert result.shape == (10, 10, 10)
    assert np.allclose(result, arr)


def test_psd3d_output_shape():
    """Verify PSD output has expected length."""
    _, psd3D = _load_psd_functions()

    arr = np.random.rand(16, 16, 16)
    x, bins = psd3D(arr, pixel_size=1.0)

    # kbins = np.arange(0.5, npix//2+1, 1.) gives npix//2 bins,
    # kvals has the same count as Abins
    assert len(x) == 16 // 2
    assert len(bins) == len(x)


def test_psd3d_positive_values():
    """PSD values should be non-negative."""
    _, psd3D = _load_psd_functions()

    arr = np.random.rand(16, 16, 16)
    x, bins = psd3D(arr, pixel_size=1.0)

    assert np.all(bins >= 0), "PSD values should be non-negative"
    assert np.all(x >= 0), "Frequency values should be non-negative"


def test_psd3d_known_signal():
    """Create array with known frequency, verify PSD peak location."""
    _, psd3D = _load_psd_functions()

    n = 32
    # Create a signal with a known frequency
    x_coord = np.arange(n)
    freq = 4  # cycles across the array
    signal_1d = np.sin(2 * np.pi * freq * x_coord / n)
    arr = np.zeros((n, n, n))
    for i in range(n):
        for j in range(n):
            arr[:, i, j] = signal_1d

    x, bins = psd3D(arr, pixel_size=1.0)

    # The peak should be near the known frequency
    peak_idx = np.argmax(bins)
    # The frequency bins are kvals/(n*pixel_size), and our signal is at freq/n
    # So the peak should be near index freq-1 (since kvals starts from 1)
    assert abs(peak_idx - (freq - 1)) <= 2, \
        f"PSD peak at index {peak_idx}, expected near {freq - 1}"


# --- FSC tests ---

def _load_fsc_functions():
    module = load_operator_module('FourierShellCorrelation')
    return module.cal_dist, module.cal_fsc, module.checkerboard_split


def test_cal_dist_2d():
    """Verify 2D distance map shape and center value."""
    cal_dist, _, _ = _load_fsc_functions()

    shape = (16, 16)
    dist_map = cal_dist(shape)

    assert dist_map.shape == shape
    # Center value should be 0 (or close to it)
    cx, cy = shape[0] // 2, shape[1] // 2
    assert dist_map[cx, cy] == 0.0


def test_cal_dist_3d():
    """Verify 3D distance map shape and center value."""
    cal_dist, _, _ = _load_fsc_functions()

    shape = (8, 8, 8)
    dist_map = cal_dist(shape)

    assert dist_map.shape == shape
    cx, cy, cz = shape[0] // 2, shape[1] // 2, shape[2] // 2
    assert dist_map[cx, cy, cz] == 0.0


def test_checkerboard_split_shape():
    """Verify checkerboard split output halves have correct shape."""
    _, _, checkerboard_split = _load_fsc_functions()

    arr = np.random.rand(16, 16, 16)
    img1, img2 = checkerboard_split(arr)

    # Each half should have half the size in each dimension
    assert img1.shape == (8, 8, 8)
    assert img2.shape == (8, 8, 8)


def test_cal_fsc_identical_images():
    """FSC of an image with itself should be ~1.0 everywhere."""
    _, cal_fsc, _ = _load_fsc_functions()

    arr = np.random.rand(16, 16)
    pixel_size = 1.0

    x, fsc, noise_onebit, noise_halfbit = cal_fsc(arr, arr, pixel_size)

    # FSC of identical images should be 1.0 (or very close)
    assert np.all(fsc > 0.99), \
        f"FSC of identical images should be ~1.0, got min={np.min(fsc)}"


def test_cal_fsc_output_lengths():
    """Verify x, fsc, noise_onebit, noise_halfbit have same length."""
    _, cal_fsc, _ = _load_fsc_functions()

    arr1 = np.random.rand(16, 16)
    arr2 = np.random.rand(16, 16)
    pixel_size = 1.0

    x, fsc, noise_onebit, noise_halfbit = cal_fsc(arr1, arr2, pixel_size)

    assert len(x) == len(fsc)
    assert len(x) == len(noise_onebit)
    assert len(x) == len(noise_halfbit)


def test_cal_fsc_noise_curves_reasonable():
    """Noise curves should be between 0 and 1."""
    _, cal_fsc, _ = _load_fsc_functions()

    arr1 = np.random.rand(16, 16)
    arr2 = np.random.rand(16, 16)
    pixel_size = 1.0

    x, fsc, noise_onebit, noise_halfbit = cal_fsc(arr1, arr2, pixel_size)

    # Skip first element which may be edge case
    assert np.all(noise_onebit[1:] >= 0), "One-bit noise should be >= 0"
    assert np.all(noise_onebit[1:] <= 1), "One-bit noise should be <= 1"
    assert np.all(noise_halfbit[1:] >= 0), "Half-bit noise should be >= 0"
    assert np.all(noise_halfbit[1:] <= 1), "Half-bit noise should be <= 1"
