def transform_scalars(dataset):
    """Calaulate 3D Sobel filter of dataset."""
    """Sobel Filter hgihlights high intensity variations."""

    from tomviz import utils
    import scipy.ndimage

    array = utils.get_array(dataset)

    # transform the dataset
    #result = scipy.ndimage.filters.sobel(array,axis=-Filter_AXIS)
    result = scipy.ndimage.filters.generic_gradient_magnitude(array, scipy.ndimage.filters.sobel)
    # set the result as the new scalars.
    utils.set_array(dataset, result)