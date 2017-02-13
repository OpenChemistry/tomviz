def transform_scalars(dataset):
    """Calculate 3D gradient magnitude using Sobel operator"""

    from tomviz import utils
    import numpy as np
    import scipy.ndimage

    array = utils.get_array(dataset)
    array = array.astype(np.float32)

    # Transform the dataset
    result = np.empty_like(array)
    scipy.ndimage.filters.generic_gradient_magnitude(
        array, scipy.ndimage.filters.sobel, output=result)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
