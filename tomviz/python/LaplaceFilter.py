def transform_scalars(dataset):
    """Apply a Laplace filter to dataset."""

    from tomviz import utils
    import scipy.ndimage
    import numpy as np
    array = utils.get_array(dataset)

    # Transform the dataset
    result = np.empty_like(array)
    scipy.ndimage.filters.laplace(array, output=result)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
