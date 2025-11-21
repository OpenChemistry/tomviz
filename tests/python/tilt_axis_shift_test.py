import numpy as np

from utils import load_operator_class, load_operator_module

from tomviz.external_dataset import Dataset


def test_tilt_axis_shift(hxn_xrf_example_dataset: Dataset,
                         tilt_axis_shift_reference_output: dict[str, np.ndarray]):
    dataset = hxn_xrf_example_dataset
    reference_output = tilt_axis_shift_reference_output

    # Load the operator module
    module = load_operator_module('AutoTiltAxisShiftAlignment')
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
        padding=10,
        num_slices=5,
        seed=0,
        apply_to_all_arrays=True,
    )

    # Verify the keys match
    assert list(sorted(reference_output)) == list(sorted(dataset.arrays))

    # Arrays should now match
    assert all(
        np.allclose(reference_output[key], dataset.arrays[key])
        for key in reference_output
    )

    # Save a new reference output:
    # np.savez_compressed('test_tilt_axis_shift_reference_output.npz', **dataset.arrays)
