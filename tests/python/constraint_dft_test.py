import numpy as np

from utils import load_operator_class, load_operator_module

from tomviz.external_dataset import Dataset


def test_constraint_dft(hxn_xrf_example_dataset: Dataset,
                        constraint_dft_reference_output: dict[str, np.ndarray]):
    dataset = hxn_xrf_example_dataset
    reference_output = constraint_dft_reference_output

    # Load the operator module
    module = load_operator_module('Recon_DFT_constraint')
    operator = load_operator_class(module)

    # Verify the keys match
    assert list(sorted(reference_output)) == list(sorted(dataset.arrays))

    # But the arrays should not match. In fact, the shapes should not match.
    assert not all(
        np.array_equal(reference_output[key].shape, dataset.arrays[key].shape)
        for key in reference_output
    )

    # Apply the transformation
    results = operator.transform(
        dataset,
        Niter=10,
        Niter_update_support=50,
        supportSigma=0.1,
        supportThreshold=10,
        seed=0,
    )

    recon_dataset = results['reconstruction']

    # Verify the keys match
    assert list(sorted(reference_output)) == list(sorted(recon_dataset.arrays))

    # Arrays should now match
    assert all(
        np.allclose(reference_output[key], recon_dataset.arrays[key], atol=1e-2)
        for key in reference_output
    )

    # Save a new reference output:
    # np.savez_compressed('test_constraint_dft_reference_output.npz', **recon_dataset.arrays)
