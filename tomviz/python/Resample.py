from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset, resampling_factor=[1, 1, 1]):
    """Resample dataset"""

    from tomviz import utils
    import scipy.ndimage
    import numpy as np

    array = dataset.active_scalars

    # Transform the dataset.
    result_shape = utils.zoom_shape(array, resampling_factor)
    result = np.empty(result_shape, array.dtype, order='F')
    scipy.ndimage.interpolation.zoom(array, resampling_factor, output=result)

    # Set the result as the new scalars.
    dataset.active_scalars = result

    # Update tilt angles if dataset is a tilt series.
    if resampling_factor[2] != 1:
        try:
            tilt_angles = dataset.tilt_angles

            result_shape = utils.zoom_shape(tilt_angles, resampling_factor[2])
            result = np.empty(result_shape, array.dtype, order='F')
            scipy.ndimage.interpolation.zoom(
                tilt_angles, resampling_factor[2], output=result)
            dataset.tilt_angles = result
        except: # noqa
            # TODO What exception are we ignoring?
            pass
