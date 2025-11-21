import numpy as np

from utils import load_operator_class, load_operator_module

from tomviz.external_dataset import Dataset


def test_multi_arrays(hxn_xrf_example_dataset: Dataset):
    dataset = hxn_xrf_example_dataset

    # Test a few operators that use `@apply_to_each_array`, and verify
    # that they do indeed apply to each array.
    # Load the operator module
    bin_module = load_operator_module('BinTiltSeriesByTwo')

    # Verify the tilt angles won't change, but image shape decreases by 2x
    orig_shapes = {k: v.shape for k, v in dataset.arrays.items()}
    orig_tilt_angles = dataset.tilt_angles

    # Apply the transformation
    bin_module.transform(dataset)

    # New shapes should be binned by 2 in x and y, but not z
    # Tilt angles should be unaffected
    binned_shapes = {k: v.shape for k, v in dataset.arrays.items()}
    expected_shapes = {k: (shape[0] // 2, shape[1] // 2, shape[2])
                       for k, shape in orig_shapes.items()}

    assert binned_shapes == expected_shapes
    assert np.allclose(orig_tilt_angles, dataset.tilt_angles)

    # Delete 3 slices. This will delete tilt angles too.
    delete_slices_module = load_operator_module('DeleteSlices')

    first_slice = 3
    last_slice = 5
    num_deleted = last_slice - first_slice + 1
    delete_slices_module.transform(dataset,
                                   firstSlice=first_slice,
                                   lastSlice=last_slice,
                                   axis=2)

    # New shapes should be the same, other than 3 less in Z
    expected_shapes = {k: (shape[0], shape[1], shape[2] - num_deleted)
                       for k, shape in binned_shapes.items()}
    deleted_shapes = {k: v.shape for k, v in dataset.arrays.items()}

    expected_angles = np.hstack((orig_tilt_angles[:first_slice],
                                 orig_tilt_angles[last_slice + 1:]))

    assert deleted_shapes == expected_shapes
    assert np.allclose(dataset.tilt_angles, expected_angles)

    # Now run a reconstruction and verify we get all results
    recon_sirt_module = load_operator_module('Recon_SIRT')
    operator = load_operator_class(recon_sirt_module)

    results = operator.transform(dataset, Niter=2)

    recon_dataset = results['reconstruction']

    # We should have the same scalar names
    assert sorted(recon_dataset.scalars_names) == sorted(dataset.scalars_names)

    # Verify the output appears valid. We are not testing the actual
    # reconstruction operator here, just that it ran on multiple arrays.
    means = [np.mean(array) for array in recon_dataset.arrays.values()]
    assert np.all(np.array(means) > 0)
