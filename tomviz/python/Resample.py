def transform_scalars(dataset):
    """Resample dataset"""

    from tomviz import utils
    import scipy.ndimage

    #----USER SPECIFIED VARIABLES-----#
    #resampling_factor  = [1,1,1]  #Specify the shifts (x,y,z) applied to data
    ###resampling_factor###
    #---------------------------------#
    array = utils.get_array(dataset)

    # Transform the dataset.
    result = scipy.ndimage.interpolation.zoom(array, resampling_factor)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)

    # Update tilt angles if dataset is a tilt series.
    if resampling_factor[2] != 1:
        try:
            tilt_angles = utils.get_tilt_angles(dataset)
            tilt_angles = scipy.ndimage.interpolation.zoom(
                tilt_angles, resampling_factor[2])
            utils.set_tilt_angles(dataset, tilt_angles)
        except: # noqa
            # TODO What exception are we ignoring?
            pass
