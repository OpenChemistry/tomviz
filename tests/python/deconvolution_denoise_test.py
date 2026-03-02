from unittest.mock import patch

import numpy as np

from utils import load_operator_class, load_operator_module

from tomviz.external_dataset import Dataset


def deep_copy_dataset(dataset):
    """Create a deep copy of a Dataset."""
    new_arrays = {name: array.copy() for name, array in dataset.arrays.items()}
    new_dataset = Dataset(new_arrays, dataset.active_name)
    if dataset.tilt_angles is not None:
        new_dataset.tilt_angles = dataset.tilt_angles.copy()
    if dataset.scan_ids is not None:
        new_dataset.scan_ids = dataset.scan_ids.copy()
    if dataset.tilt_axis is not None:
        new_dataset.tilt_axis = dataset.tilt_axis
    new_dataset.metadata = dataset.metadata.copy() if dataset.metadata else {}
    return new_dataset


def test_deconvolution_denoise(chipset_xrf_dataset, chipset_probe_dataset):
    xrf_dataset = chipset_xrf_dataset
    probe_dataset = chipset_probe_dataset

    # Delete slices to keep only 3 (reduces runtime significantly)
    delete_slices = load_operator_module('DeleteSlices')

    n_slices_xrf = xrf_dataset.active_scalars.shape[2]
    if n_slices_xrf > 3:
        delete_slices.transform(xrf_dataset,
                                firstSlice=3,
                                lastSlice=n_slices_xrf - 1,
                                axis=2)

    n_slices_probe = probe_dataset.active_scalars.shape[2]
    if n_slices_probe > 3:
        delete_slices.transform(probe_dataset,
                                firstSlice=3,
                                lastSlice=n_slices_probe - 1,
                                axis=2)

    # Set scan_ids to None for index-based matching (scan_ids are not
    # updated by DeleteSlices, so use index-based matching instead)
    xrf_dataset.scan_ids = None
    probe_dataset.scan_ids = None

    # Save a copy of the original XRF dataset for reference
    original_xrf = deep_copy_dataset(xrf_dataset)

    # Record original shapes for later verification
    original_shapes = {name: array.shape
                       for name, array in xrf_dataset.arrays.items()}

    # Run deconvolution denoise with APG_TV method and 2x scaling
    deconv_module = load_operator_module('DeconvolutionDenoise')
    deconv_operator = load_operator_class(deconv_module)

    all_scalars = tuple(xrf_dataset.scalars_names)

    deconv_operator.transform(
        xrf_dataset,
        probe=probe_dataset,
        selected_scalars=all_scalars,
        method="APG_TV",
        scale_x=2,
        scale_y=2,
    )

    # Verify output is 2x larger in x and y, same in z
    for name in xrf_dataset.scalars_names:
        output_shape = xrf_dataset.scalars(name).shape
        orig_shape = original_shapes[name]
        assert output_shape[0] == orig_shape[0] * 2, \
            f"{name}: expected x={orig_shape[0] * 2}, got {output_shape[0]}"
        assert output_shape[1] == orig_shape[1] * 2, \
            f"{name}: expected y={orig_shape[1] * 2}, got {output_shape[1]}"
        assert output_shape[2] == orig_shape[2], \
            f"{name}: expected z={orig_shape[2]}, got {output_shape[2]}"

    # Run similarity metrics: deconvolution output vs original
    sim_module = load_operator_module('SimilarityMetrics')

    # Mock make_spreadsheet since it requires VTK (internal mode only)
    deconv_metrics = {}

    def capture_deconv_spreadsheet(column_names, table_data, *args, **kwargs):
        deconv_metrics['column_names'] = list(column_names)
        deconv_metrics['data'] = table_data.copy()
        return deconv_metrics

    sim_operator = load_operator_class(sim_module)
    with patch('tomviz.utils.make_spreadsheet', capture_deconv_spreadsheet):
        sim_operator.transform(
            xrf_dataset,
            reference_dataset=original_xrf,
        )

    # Gaussian blur on a copy of the original
    gaussian_xrf = deep_copy_dataset(original_xrf)
    gaussian_module = load_operator_module('GaussianFilter')
    gaussian_module.transform(gaussian_xrf, sigma=2.0)

    # Run similarity metrics: gaussian output vs original
    gaussian_metrics = {}

    def capture_gaussian_spreadsheet(column_names, table_data, *args, **kwargs):
        gaussian_metrics['column_names'] = list(column_names)
        gaussian_metrics['data'] = table_data.copy()
        return gaussian_metrics

    sim_operator2 = load_operator_class(sim_module)
    with patch('tomviz.utils.make_spreadsheet', capture_gaussian_spreadsheet):
        sim_operator2.transform(
            gaussian_xrf,
            reference_dataset=original_xrf,
        )

    # Compare metrics - deconvolution should be clearly better on average
    # (lower MSE and higher SSIM)
    deconv_data = deconv_metrics['data']
    gaussian_data = gaussian_metrics['data']
    deconv_columns = deconv_metrics['column_names']
    gaussian_columns = gaussian_metrics['column_names']

    # Collect all MSE and SSIM values across scalars
    deconv_mse_values = []
    gauss_mse_values = []
    deconv_ssim_values = []
    gauss_ssim_values = []

    for i, col_name in enumerate(deconv_columns):
        if 'MSE' in col_name:
            deconv_mse_values.append(np.mean(deconv_data[:, i]))
            gauss_idx = gaussian_columns.index(col_name)
            gauss_mse_values.append(np.mean(gaussian_data[:, gauss_idx]))

        elif 'SSIM' in col_name:
            deconv_ssim_values.append(np.mean(deconv_data[:, i]))
            gauss_idx = gaussian_columns.index(col_name)
            gauss_ssim_values.append(np.mean(gaussian_data[:, gauss_idx]))

    # Average across all scalars - deconv should be better overall
    avg_deconv_mse = np.mean(deconv_mse_values)
    avg_gauss_mse = np.mean(gauss_mse_values)
    avg_deconv_ssim = np.mean(deconv_ssim_values)
    avg_gauss_ssim = np.mean(gauss_ssim_values)

    assert avg_deconv_mse < avg_gauss_mse, \
        (f"Avg deconv MSE ({avg_deconv_mse:.6f}) should be lower than "
         f"avg Gaussian MSE ({avg_gauss_mse:.6f})")
    assert avg_deconv_ssim > avg_gauss_ssim, \
        (f"Avg deconv SSIM ({avg_deconv_ssim:.6f}) should be higher than "
         f"avg Gaussian SSIM ({avg_gauss_ssim:.6f})")
