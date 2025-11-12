from pathlib import Path

import numpy as np

from utils import download_and_unzip_file, load_operator_module

from tomviz.executor import load_dataset


def test_pystackreg(hxn_xrf_example_output_dir: Path,
                    pystackreg_reference_output: dict[str, np.ndarray]):
    # Load two files so we can test applying to all arrays
    example_files = [
        'Pt_L.h5',
        'Zn_K.h5',
    ]
    example_files = [
        hxn_xrf_example_output_dir / 'extracted_elements' / name
        for name in example_files
    ]
    dataset = load_dataset(example_files[0])
    for new_file in example_files[1:]:
        new_dataset = load_dataset(new_file)
        new_name = new_dataset.active_name
        dataset.arrays[new_name] = new_dataset.active_scalars

    # Load the operator module
    module = load_operator_module('PyStackRegImageAlignment')

    # Verify the keys match
    assert list(sorted(pystackreg_reference_output)) == list(sorted(dataset.arrays))

    # But the arrays should not match
    assert not all(
        np.allclose(pystackreg_reference_output[key], dataset.arrays[key])
        for key in pystackreg_reference_output
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
