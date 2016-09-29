def transform_scalars(dataset):
    """Downsample tilt images by a factor of 2"""

    from tomviz import utils
    import scipy.ndimage

    array = utils.get_array(dataset)

    # Downsample the dataset x2 using order 1 spline (linear)
    result = scipy.ndimage.interpolation.zoom(array,
                (0.5, 0.5, 1),output=None, order=1,
                mode='constant', cval=0.0, prefilter=False)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
