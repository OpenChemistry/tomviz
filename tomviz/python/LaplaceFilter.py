def transform_scalars(dataset):
    """Apply a Laplace filter to dataset."""

    from tomviz import utils
    import numpy as np
    import scipy.ndimage

    array = utils.get_array(dataset)

    # Transform the dataset
    result = scipy.ndimage.filters.laplace(array)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
