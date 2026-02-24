import numpy as np

from utils import load_operator_module

from tomviz.external_dataset import Dataset


def _make_dataset(num_arrays=3, shape=(10, 10, 5)):
    """Create a dataset with multiple named scalar arrays."""
    arrays = {}
    for i in range(num_arrays):
        arr = np.random.RandomState(i).rand(*shape).astype(np.float32)
        arrays[f'array_{i}'] = arr

    dataset = Dataset(arrays)
    dataset.spacing = [1.0, 1.0, 1.0]
    dataset.tilt_angles = np.linspace(-70, 70, shape[2])
    return dataset


def test_remove_arrays_keep_one():
    """Test keeping a single array removes all others."""
    module = load_operator_module('RemoveArrays')

    dataset = _make_dataset(num_arrays=3)
    assert len(dataset.scalars_names) == 3

    module.transform(dataset, selected_scalars=('array_1',))

    assert dataset.scalars_names == ['array_1']
    assert len(dataset.arrays) == 1


def test_remove_arrays_keep_multiple():
    """Test keeping multiple arrays removes only unselected ones."""
    module = load_operator_module('RemoveArrays')

    dataset = _make_dataset(num_arrays=4)
    assert len(dataset.scalars_names) == 4

    module.transform(dataset, selected_scalars=('array_0', 'array_2'))

    assert sorted(dataset.scalars_names) == ['array_0', 'array_2']
    assert len(dataset.arrays) == 2


def test_remove_arrays_keep_all():
    """Test that selecting all arrays removes nothing."""
    module = load_operator_module('RemoveArrays')

    dataset = _make_dataset(num_arrays=3)
    originals = {name: arr.copy() for name, arr in dataset.arrays.items()}

    module.transform(
        dataset,
        selected_scalars=('array_0', 'array_1', 'array_2'),
    )

    assert sorted(dataset.scalars_names) == sorted(originals.keys())
    for name in dataset.scalars_names:
        assert np.array_equal(dataset.arrays[name], originals[name])


def test_remove_arrays_data_preserved():
    """Test that kept arrays retain their original data."""
    module = load_operator_module('RemoveArrays')

    dataset = _make_dataset(num_arrays=3)
    kept_original = dataset.arrays['array_1'].copy()

    module.transform(dataset, selected_scalars=('array_1',))

    assert np.array_equal(dataset.arrays['array_1'], kept_original)


def test_remove_arrays_default_keeps_active():
    """Test that passing None keeps only the active scalar array."""
    module = load_operator_module('RemoveArrays')

    dataset = _make_dataset(num_arrays=3)
    active_name = dataset.active_name

    module.transform(dataset, selected_scalars=None)

    assert dataset.scalars_names == [active_name]
