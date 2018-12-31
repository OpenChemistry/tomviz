def transform_scalars(dataset):
    """Downsample tilt images by a factor of 2"""

    from tomviz import utils
    import scipy.ndimage
    import numpy as np
    import warnings

    array = utils.get_array(dataset)

    zoom = (0.5, 0.5, 1)
    result_shape = utils.zoom_shape(array, zoom)
    result = np.empty(result_shape, array.dtype, order='F')
    # Downsample the dataset x2 using order 1 spline (linear)
    warnings.filterwarnings('ignore', '.*output shape of zoom.*')
    scipy.ndimage.interpolation.zoom(array, zoom,
                                     output=result,
                                     order=1,
                                     mode='constant',
                                     cval=0.0, prefilter=False)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
