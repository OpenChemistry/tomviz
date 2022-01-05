def transform(moving_dataset, fixed_dataset, max_num_iterations,
              num_resolutions, disable_rotation, interpolator,
              bspline_interpolation_order, resample_interpolator,
              resample_bspline_interpolation_order, lower_threshold,
              upper_threshold, show_elastix_console_output):

    try:
        from tomviz import itkutils
        import itk
        from itk import elastix_registration_method
        import numpy as np
    except Exception:
        print('Could not import the necessary modules')
        raise

    def make_itk_image(array, dataset):
        # Use this utility function so we can modify the array on the dataset
        image = itk.GetImageViewFromArray(array)
        image.SetSpacing(dataset.spacing)
        return image

    moving_array = moving_dataset.active_scalars
    fixed_array = fixed_dataset.active_scalars

    # The datatype must be float32, or elastix will crash
    required_dtype = np.float32
    if moving_array.dtype != required_dtype:
        moving_array = moving_array.astype(required_dtype)
    if fixed_array.dtype != required_dtype:
        fixed_array = fixed_array.astype(required_dtype)

    moving_image = make_itk_image(moving_array, moving_dataset)
    fixed_image = make_itk_image(fixed_array, fixed_dataset)

    mask_type = itk.Image[itk.UC, 3]
    kwargs = {
        'lower_threshold': lower_threshold,
        'upper_threshold': upper_threshold,
        'inside_value': 1,
        'ttype': (type(moving_image), mask_type),
    }
    moving_mask = itk.binary_threshold_image_filter(moving_image, **kwargs)

    kwargs = {
        'lower_threshold': lower_threshold,
        'upper_threshold': upper_threshold,
        'inside_value': 1,
        'ttype': (type(fixed_image), mask_type),
    }
    fixed_mask = itk.binary_threshold_image_filter(fixed_image, **kwargs)

    method = 'translation' if disable_rotation else 'rigid'
    parameter_object = itk.ParameterObject.New()
    default_rigid_parameter_map = parameter_object.GetDefaultParameterMap(
        method)
    parameter_object.AddParameterMap(default_rigid_parameter_map)

    modify_parameters_map = {
        'MaximumNumberOfIterations': max_num_iterations,
        'NumberOfResolutions': num_resolutions,
        'Interpolator': interpolator,
        'BSplineInterpolationOrder': bspline_interpolation_order,
        'ResampleInterpolator': resample_interpolator,
        'FinalBSplineInterpolationOrder': resample_bspline_interpolation_order,
    }
    for k, v in modify_parameters_map.items():
        parameter_object.SetParameter(0, k, str(v))

    # Call registration function
    kwargs = {
        'parameter_object': parameter_object,
        'fixed_mask': fixed_mask,
        'moving_mask': moving_mask,
        'log_to_console': show_elastix_console_output,
    }
    result_image, result_transform_parameters = elastix_registration_method(
        fixed_image, moving_image, **kwargs)

    itkutils.set_itk_image_on_dataset(result_image, moving_dataset)
