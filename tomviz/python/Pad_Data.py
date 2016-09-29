def transform_scalars(dataset):
    """Pad dataset"""
    from tomviz import utils
    import numpy as np

    #----USER SPECIFIED VARIABLES-----#
    ###padWidthX###
    ###padWidthY###
    ###padWidthZ###
    ###padMode_index###
    #---------------------------------#
    padModes = ['constant', 'edge', 'wrap', 'minimum', 'median']
    padMode = padModes[padMode_index]
    array = utils.get_array(dataset) #get data as numpy array

    if array is None: #Check if data exists
        raise RuntimeError("No data array found!")

    pad_width = (padWidthX, padWidthY, padWidthZ)

    # pad the data.
    array = np.lib.pad(array, pad_width, padMode)

    # Set the data so that it is visible in the application.
    utils.set_array(dataset, array)

    # If dataset is marked as tilt series, update tilt angles
    if padWidthZ[0] + padWidthZ[1] > 0:
        try:
            tilt_angles = utils.get_tilt_angles(dataset)
            tilt_angles = np.lib.pad(tilt_angles, padWidthZ, padMode)
            utils.set_tilt_angles(dataset, tilt_angles)
        except:
            pass
