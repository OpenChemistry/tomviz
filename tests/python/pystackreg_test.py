import numpy as np

from utils import load_operator_module

from tomviz.external_dataset import Dataset


def test_pystackreg(hxn_xrf_example_dataset: Dataset,
                    pystackreg_reference_output: dict[str, np.ndarray]):
    dataset = hxn_xrf_example_dataset
    reference_output = pystackreg_reference_output

    # Load the operator module
    module = load_operator_module('PyStackRegImageAlignment')

    # Verify the keys match
    assert list(sorted(reference_output)) == list(sorted(dataset.arrays))

    # But the arrays should not match
    assert not all(
        np.allclose(reference_output[key], dataset.arrays[key])
        for key in reference_output
    )

    # Apply the transformation
    module.transform(
        dataset,
        transform_source='generate',
        padding=10,
        apply_to_all_arrays=True,
        # Only used if `transform_source` is `generate`
        transform_type='translation',
        reference='slice_index',
        transforms_save_file='',
        # Only used if `transform_type` is `slice_index`
        ref_slice_index=90,
        # Only used if `transform_source` is `from_file`.
        transform_file=None,
    )

    # Verify the keys match
    assert list(sorted(pystackreg_reference_output)) == list(sorted(dataset.arrays))

    # Arrays should now match
    assert all(
        np.allclose(pystackreg_reference_output[key], dataset.arrays[key])
        for key in pystackreg_reference_output
    )

    # Save a new reference output:
    # np.savez_compressed('test_pystackreg_reference_output.npz', **dataset.arrays)
