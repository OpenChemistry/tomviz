def transform_scalars(dataset):
    """Resample dataset"""

    from tomviz import utils
    import scipy.ndimage

    #----USER SPECIFIED VARIABLES-----#
    #resampingFactor  = [1,1,1]  #Specify the shifts (x,y,z) applied to data
    ###resampingFactor###
    #---------------------------------#
    array = utils.get_array(dataset)

    # Transform the dataset.
    result = scipy.ndimage.interpolation.zoom(array, resampingFactor)
    
    # Set the result as the new scalars.
    utils.set_array(dataset, result)

    # Update tilt angles if dataset is a tilt series.
    if resampingFactor[2] != 1:
        try:
            tilt_angles = utils.get_tilt_angles(dataset)
            tilt_angles = scipy.ndimage.interpolation.zoom(tilt_angles, resampingFactor[2])
            utils.set_tilt_angles(dataset, tilt_angles)
        except:
            pass