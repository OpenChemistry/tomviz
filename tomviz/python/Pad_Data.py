def transform_scalars(dataset, pad_size_before=[0, 0, 0],
                      pad_size_after=[0, 0, 0], pad_mode_index=0):

    """Pad dataset"""
    from tomviz import utils
    import numpy as np

    padModes = ['constant', 'edge', 'wrap', 'minimum', 'median']
    padMode = padModes[pad_mode_index]
    array = utils.get_array(dataset) #get data as numpy array

    if array is None: #Check if data exists
        raise RuntimeError("No data array found!")

    padWidthX = (pad_size_before[0], pad_size_after[0])
    padWidthY = (pad_size_before[1], pad_size_after[1])
    padWidthZ = (pad_size_before[2], pad_size_after[2])

    pad_width = (padWidthX, padWidthY, padWidthZ)

    # pad the data.
    array = np.lib.pad(array, pad_width, padMode)

    # Set the data so that it is visible in the application.
    extent = list(dataset.GetExtent())
    start = [x - y for (x, y) in zip(extent[0::2], pad_size_before)]

    utils.set_array(dataset, array, start)

    # If dataset is marked as tilt series, update tilt angles
    if padWidthZ[0] + padWidthZ[1] > 0:
        try:
            tilt_angles = utils.get_tilt_angles(dataset)
            tilt_angles = np.lib.pad(tilt_angles, padWidthZ, padMode)
            utils.set_tilt_angles(dataset, tilt_angles)
        except: # noqa
            # TODO What exception are we ignoring?
            pass
