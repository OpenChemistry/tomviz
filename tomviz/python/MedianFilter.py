def transform_scalars(dataset, size=2):
    """Apply a Median filter to dataset."""
    """ Median filter is a nonlinear filter used to reduce noise."""

    from tomviz import utils
    import scipy.ndimage
    import numpy as np

    array = utils.get_array(dataset)

    # Transform the dataset.
    result = np.empty_like(array)
    scipy.ndimage.filters.median_filter(array, size, output=result)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
