import numpy as np
from scipy import ndimage

from utils import load_operator_class, load_operator_module

from tomviz_pipeline.dataset import Dataset


def test_xcorr(hxn_xrf_example_dataset: Dataset,
               xcorr_reference_output: dict[str, np.ndarray]):
    dataset = hxn_xrf_example_dataset
    reference_output = xcorr_reference_output

    # Load the operator module
    module = load_operator_module('AutoCrossCorrelationTiltImageAlignment')
    operator = load_operator_class(module)

    # Verify the keys match
    assert list(sorted(reference_output)) == list(sorted(dataset.arrays))

    # But the arrays should not match
    assert not all(
        np.allclose(reference_output[key], dataset.arrays[key])
        for key in reference_output
    )

    # Apply the transformation
    operator.transform(
        dataset,
        transform_source='generate',
        apply_to_all_arrays=True,
    )

    # Verify the keys match
    assert list(sorted(reference_output)) == list(sorted(dataset.arrays))

    # The reference wrapped the images around their edges. Folding the
    # padded output back onto the original size should give the same.
    # (Only for the active array: the reference shifted the others a
    # pixel off whenever a shift was negative.)
    key = dataset.active_name
    assert np.allclose(reference_output[key],
                       _fold(dataset.arrays[key], reference_output[key].shape))

    # Save a new reference output:
    # np.savez_compressed('test_xcorr_reference_output.npz', **dataset.arrays)


def _fold(padded: np.ndarray, shape: tuple[int, ...]) -> np.ndarray:
    """Wrap an image padded equally on both sides back onto shape."""
    rows, cols, count = shape
    pad_rows = (padded.shape[0] - rows) // 2
    pad_cols = (padded.shape[1] - cols) // 2
    i = (np.arange(padded.shape[0]) - pad_rows) % rows
    j = (np.arange(padded.shape[1]) - pad_cols) % cols
    folded = np.zeros(shape)
    np.add.at(folded, (i[:, None, None], j[None, :, None],
                       np.arange(count)[None, None, :]), padded)
    return folded


def test_xcorr_keeps_a_sample_that_leaves_the_view(tmp_path) -> None:
    # A sample taller than the view, drifting up and down between tilts
    rng = np.random.default_rng(0)
    sample = ndimage.gaussian_filter(rng.random((60, 40)), 2)
    drifts = [-3, 0, 4]
    views = [sample[6 - d:54 - d] for d in drifts]
    data = np.stack(views, axis=2)
    dataset = Dataset({'data': data, 'double': data * 2}, 'data')
    dataset.spacing = (1.0, 1.0, 1.0)

    module = load_operator_module('AutoCrossCorrelationTiltImageAlignment')
    operator = load_operator_class(module)
    shifts_file = tmp_path / 'shifts.npz'
    operator.transform(dataset, transforms_save_file=str(shifts_file))

    # Padded by the largest drift, so the edges did not wrap around
    out = dataset.active_scalars
    assert out.shape == (48 + 2 * 4, 40, 3)
    # Wherever two images both have data, they show the same thing
    middle = out[:, :, 1]
    for i in (0, 2):
        both = (middle != 0) & (out[:, :, i] != 0)
        assert np.allclose(out[:, :, i][both], middle[both])
    # Every array is shifted the same way
    assert np.allclose(dataset.scalars('double'), out * 2)
    assert np.array_equal(np.load(shifts_file)['offsets'][:, 0], [3, 0, -4])

    # The saved shifts apply the same way, also as earlier versions saved
    # them (0 to N - 1 instead of signed)
    unsigned_file = tmp_path / 'unsigned.npz'
    with np.load(shifts_file) as f:
        np.savez(unsigned_file, offsets=f['offsets'] % [48, 40],
                 spacing=f['spacing'])
    for path in (shifts_file, unsigned_file):
        again = Dataset({'data': data}, 'data')
        again.spacing = (1.0, 1.0, 1.0)
        operator.transform(again, transform_source='from_file',
                           transform_file=str(path))
        assert np.allclose(again.active_scalars, out)
