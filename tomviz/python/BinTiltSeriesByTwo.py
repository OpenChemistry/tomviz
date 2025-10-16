from tomviz import utils


@utils.apply_to_each_array
def transform(dataset):
    """Downsample tilt images by a factor of 2"""

    import scipy.ndimage
    import numpy as np
    import warnings

    array = dataset.active_scalars

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
    dataset.active_scalars = result
