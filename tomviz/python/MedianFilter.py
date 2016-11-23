def transform_scalars(dataset, size=2):
    """Apply a Median filter to dataset."""
    """ Median filter is a nonlinear filter used to reduce noise."""

    from tomviz import utils
    import scipy.ndimage

    array = utils.get_array(dataset)

    # Transform the dataset.
    result = scipy.ndimage.filters.median_filter(array, size)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
