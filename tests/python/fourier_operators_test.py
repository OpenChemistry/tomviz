import numpy as np
import pytest

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)
from tomviz_pipeline.nodes.transforms.python_transform import (
    PythonTransform,
)

from utils import OPERATOR_PATH


# Schema-v2 operators (kernel classes) run through PythonTransform
V2_OPERATORS = {'FourierPeakMask'}


def _run_operator_raw(name, inputs, **arguments):
    t = PythonTransform() if name in V2_OPERATORS else LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / f'{name}.json').read_text(),
        'script': (OPERATOR_PATH / f'{name}.py').read_text(),
        'arguments': arguments,
    })
    ports = {}
    for port, arr in inputs.items():
        ds = Dataset({'ImageScalars': np.asfortranarray(arr)},
                     'ImageScalars')
        ds.spacing = (1.0, 1.0, 1.0)
        ports[port] = PortData(ds, 'ImageData')
    return t, t.transform(ports)


def _run_operator(name, inputs, **arguments):
    t, result = _run_operator_raw(name, inputs, **arguments)
    return result[t.output_ports()[0].name].payload.active_scalars


def _run_filter(arr, **arguments):
    return _run_operator('FourierFilter', {'volume': arr}, **arguments)


def _run_peak_mask(arr, **arguments):
    # Typed centers unless a test opts into auto-detection
    arguments.setdefault('auto_detect', False)
    return _run_operator('FourierPeakMask', {'volume': arr}, **arguments)


def _run_fourier_mask(data, mask, **arguments):
    return _run_operator('FourierMask', {'volume': data, 'mask': mask},
                         **arguments)


def _run_image_math(a, b, **arguments):
    return _run_operator('ImageMath', {'volume': a, 'second_dataset': b},
                         **arguments)


N = 32


def _wave(k):
    # A cosine with integer frequency vector k: two spectral deltas at
    # the fftshift-ed DC bin +/- k.
    x, y, z = np.mgrid[0:N, 0:N, 0:N].astype(np.float64)
    return np.cos(2 * np.pi * (k[0] * x + k[1] * y + k[2] * z) / N)


def _corr(a, b):
    a = a - a.mean()
    b = b - b.mean()
    return float((a * b).sum() / np.sqrt((a * a).sum() * (b * b).sum()))


def test_low_pass_keeps_low_and_removes_high():
    low = _wave((2, 0, 0))
    high = _wave((0, 12, 0))
    out = _run_filter(low + high, filter_type=0, cutoff=6.0, width=0.0)
    assert np.abs(out - low).max() < 1e-6
    # A soft edge between the two frequencies separates them just as well
    out = _run_filter(low + high, filter_type=0, cutoff=6.0, width=2.0)
    assert _corr(out, low) > 0.99
    # NaNs in the input must not poison the transform
    noisy = low.copy()
    noisy[0, 0, 0] = np.nan
    assert np.isfinite(_run_filter(noisy, filter_type=0, cutoff=6.0,
                                   width=0.0)).all()


def test_high_pass_is_the_complement():
    data = _wave((2, 0, 0)) + _wave((0, 12, 0)) + 3.0
    lp = _run_filter(data, filter_type=0, cutoff=6.0, width=0.0)
    hp = _run_filter(data, filter_type=1, cutoff=6.0, width=0.0)
    assert np.abs((lp + hp) - data).max() < 1e-5


def test_band_pass_keeps_only_the_shell():
    inner = _wave((2, 0, 0))
    shell = _wave((0, 12, 0))
    outer = _wave((15, 8, 0))
    for profile in range(4):  # Gaussian, Hann, Rect, Erf
        out = _run_filter(inner + shell + outer, filter_type=2, cutoff=12.0,
                          width=3.0, band_profile=profile)
        assert _corr(out, shell) > 0.98, f'profile {profile}'


def test_2d_per_slice_mode_leaves_axis_variation_alone():
    # A signal varying only along z is each slice's DC: a 2D low pass
    # over x-y slices must keep it exactly.
    along_z = _wave((0, 0, 5))
    out = _run_filter(along_z + _wave((12, 0, 0)), filter_type=0, cutoff=4.0,
                      width=0.0, dimensionality=1, slice_axis=2)
    assert np.abs(out - along_z).max() < 1e-6


def _peak_index(k):
    return tuple(N // 2 + c for c in k)


def test_peak_mask_isolates_one_component():
    k1, k2 = (5, 0, 0), (0, 9, 3)
    comp1, comp2 = _wave(k1), _wave(k2)
    center = ', '.join(str(c) for c in _peak_index(k1))
    out = _run_peak_mask(comp1 + comp2, centers=center, radius=3.0,
                         sigma=0.5)
    assert _corr(out, comp1) > 0.99
    assert abs(_corr(out, comp2)) < 0.05

    # Without the Friedel mate only half the amplitude survives
    one_sided = _run_peak_mask(comp1, centers=center, radius=3.0, sigma=0.5,
                               include_friedel_mates=False)
    both = _run_peak_mask(comp1, centers=center, radius=3.0, sigma=0.5)
    assert 0.4 < one_sided.std() / both.std() < 0.6


def test_peak_mask_centers_from_csv(tmp_path):
    k1, k2 = (5, 0, 0), (0, 9, 3)
    csv = tmp_path / 'centers.csv'
    csv.write_text('x,y,z\n' + ','.join(str(c) for c in _peak_index(k1)) +
                   '\n')
    out = _run_peak_mask(_wave(k1) + _wave(k2), centers_file=str(csv),
                         radius=3.0, sigma=0.5)
    assert _corr(out, _wave(k1)) > 0.99


def test_peak_mask_rejects_bad_centers():
    # A raising operator surfaces as an empty (failed) result
    _, result = _run_operator_raw('FourierPeakMask',
                                  {'volume': _wave((5, 0, 0))},
                                  auto_detect=False)
    assert result == {}
    _, result = _run_operator_raw('FourierPeakMask',
                                  {'volume': _wave((5, 0, 0))},
                                  auto_detect=False, centers='999, 0, 0')
    assert result == {}


def test_peak_mask_auto_detects_peaks_and_writes_them_back():
    k1, k2 = (5, 0, 0), (0, 9, 3)
    strong, weak = 2.0 * _wave(k1), 0.5 * _wave(k2)
    data = strong + weak + 1.0  # the DC term must be skipped
    t, result = _run_operator_raw('FourierPeakMask', {'volume': data},
                                  auto_detect=True, threshold=0.8,
                                  exclude_center_radius=3.0,
                                  min_peak_size=1, radius=2.0, sigma=0.5)
    out = result[t.output_ports()[0].name].payload.active_scalars
    # Both components survive, the DC offset does not
    assert _corr(out, strong + weak) > 0.99
    assert abs(out.mean()) < 1e-3
    # The peaks found land in the centers parameter, strongest first,
    # one per Friedel pair
    centers = t.parameters['centers']
    found = [tuple(int(v) for v in c.split(',')) for c in centers.split(';')]
    assert len(found) == 2
    assert found[0] in (_peak_index(k1), _peak_index((-5, 0, 0)))
    assert found[1] in (_peak_index(k2), _peak_index((0, -9, -3)))

    # Max Peaks keeps only the strongest pair
    out = _run_peak_mask(data, auto_detect=True, threshold=0.8,
                         exclude_center_radius=3.0, min_peak_size=1,
                         max_peaks=1, radius=2.0, sigma=0.5)
    assert _corr(out, strong) > 0.99
    assert abs(_corr(out, weak)) < 0.05

    # Nothing above the threshold fails the run instead of returning zeros
    _, result = _run_operator_raw('FourierPeakMask', {'volume': data},
                                  auto_detect=True, threshold=1.0)
    assert result == {}


def test_image_math_operations():
    a = np.full((4, 4, 4), 6.0)
    b = np.full((4, 4, 4), 2.0)
    assert np.allclose(_run_image_math(a, b, operation=0), 4.0)
    assert np.allclose(_run_image_math(a, b, operation=1), 8.0)
    assert np.allclose(_run_image_math(a, b, operation=2), 12.0)
    assert np.allclose(_run_image_math(a, b, operation=3), 3.0)
    # Normalizing to unit mean first makes the difference vanish
    assert np.allclose(
        _run_image_math(a, b, operation=0, normalize_first=True), 0.0)


def test_image_math_guards():
    a = np.full((4, 4, 4), 6.0)
    # Division by zero yields zero, not inf
    b = np.zeros((4, 4, 4))
    b[0, 0, 0] = 2.0
    out = _run_image_math(a, b, operation=3)
    assert out[0, 0, 0] == 3.0 and np.count_nonzero(out) == 1
    # Shape mismatches fail the run
    _, result = _run_operator_raw(
        'ImageMath', {'volume': a, 'second_dataset': np.zeros((4, 4, 5))})
    assert result == {}


def test_image_math_resamples_a_smaller_second_dataset():
    a = np.full((8, 8, 8), 5.0)
    b = np.full((4, 4, 4), 2.0)
    with pytest.raises(Exception):
        _run_image_math(a, b, operation=1)
    out = _run_image_math(a, b, operation=1, resample_to_match=True)
    assert out.shape == a.shape
    assert np.allclose(out, 7.0)


def test_fourier_mask_keeps_the_selected_component():
    # Two periodicities along x; a mask around one peak keeps only it
    n = 32
    x = np.arange(n)[:, None, None]
    data = (np.cos(2 * np.pi * 4 * x / n) +
            np.cos(2 * np.pi * 10 * x / n)) * np.ones((n, n, n))
    mask = np.zeros((n, n, n), dtype=np.uint8)
    mask[n // 2 + 4, n // 2, n // 2] = 1  # +4 cycles; the mate is added
    out = _run_fourier_mask(data, mask, edge_softness=0.0)
    expected = np.cos(2 * np.pi * 4 * x / n) * np.ones((n, n, n))
    assert np.allclose(out, expected, atol=1e-3)
    inverted = _run_fourier_mask(data, mask, edge_softness=0.0, invert=True)
    assert np.allclose(inverted, data - expected, atol=1e-3)


def test_fourier_mask_rejects_a_mismatched_or_empty_mask():
    data = np.ones((8, 8, 8))
    with pytest.raises(Exception):
        _run_fourier_mask(data, np.ones((4, 4, 4), dtype=np.uint8))
    with pytest.raises(Exception):
        _run_fourier_mask(data, np.zeros((8, 8, 8), dtype=np.uint8))


def test_image_math_pads_an_off_by_one_second_dataset():
    a = np.full((8, 8, 8), 5.0)
    b = np.full((8, 9, 7), 2.0)
    out = _run_image_math(a, b, operation=1, resample_to_match=True)
    assert out.shape == a.shape
    assert np.allclose(out, 7.0)
