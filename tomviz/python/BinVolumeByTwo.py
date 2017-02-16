def transform_scalars(dataset):
    """Downsample volume by a factor of 2"""

    from tomviz import utils
    import scipy.ndimage
    import numpy as np

    array = utils.get_array(dataset)

    # Downsample the dataset x2 using order 1 spline (linear)
    # Calculate out array shape
    zoom = (0.5, 0.5, 0.5)
    result_shape = utils.zoom_shape(array, zoom)
    result = np.empty(result_shape, array.dtype, order='F')
    scipy.ndimage.interpolation.zoom(array, zoom,
                                     output=result, order=1,
                                     mode='constant', cval=0.0,
                                     prefilter=False)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)

    # Update tilt angles if dataset is a tilt series.
    try:
        tilt_angles = utils.get_tilt_angles(dataset)
        result_shape = utils.zoom_shape(tilt_angles, 0.5)
        result = np.empty(result_shape, array.dtype, order='F')
        tilt_angles = scipy.ndimage.interpolation.zoom(tilt_angles, 0.5,
                                                       output=result)
        utils.set_tilt_angles(dataset, result)
    except: # noqa
        # TODO What exception are we ignoring?
        pass
