def transform_scalars(dataset):
    """Downsample dataset by a factor of 2"""

    from tomviz import utils
    import numpy as np
    import scipy.ndimage

    array = utils.get_array(dataset)

    # Transform the dataset.
    result = scipy.ndimage.interpolation.zoom(array, (0.5, 0.5, 0.5))

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
