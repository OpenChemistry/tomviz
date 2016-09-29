def transform_scalars(dataset):
    """Downsample volume by a factor of 2"""

    from tomviz import utils
    import scipy.ndimage

    array = utils.get_array(dataset)

    # Downsample the dataset x2 using order 1 spline (linear)
    result = scipy.ndimage.interpolation.zoom(array, 
                (0.5, 0.5, 0.5),output=None, order=1,
                mode='constant', cval=0.0, prefilter=False)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)

    # Update tilt angles if dataset is a tilt series.
    try:
        tilt_angles = utils.get_tilt_angles(dataset)
        tilt_angles = scipy.ndimage.interpolation.zoom(tilt_angles, 0.5)
        utils.set_tilt_angles(dataset, tilt_angles)
    except:
        pass