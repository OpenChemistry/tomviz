def transform_scalars(dataset):
    """Apply 3D Sobel filter to dataset"""
    """Sobel Filter hgihlights high intensity variations"""

    from tomviz import utils
    import scipy.ndimage

    array = utils.get_array(dataset)

    # transform the dataset
    result = scipy.ndimage.filters.generic_gradient_magnitude(array, scipy.ndimage.filters.sobel)
    
    # set the result as the new scalars.
    utils.set_array(dataset, result)