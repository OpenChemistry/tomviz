def transform_scalars(dataset, firstSlice=None, lastSlice=None, axis=2):
    """Delete Slices in Dataset"""

    from tomviz import utils
    import numpy as np

    # Get the current dataset.
    array = utils.get_array(dataset)

    # Get indices of the slices to be deleted.
    indices = np.linspace(firstSlice, lastSlice,
                          lastSlice - firstSlice + 1).astype(int)

    # Delete the specified slices.
    array = np.delete(array, indices, axis)

    # Set the result as the new scalars.
    utils.set_array(dataset, array)

    # Delete corresponding tilt anlges if dataset is a tilt series.
    if axis == 2:
        try:
            tilt_angles = utils.get_tilt_angles(dataset)
            tilt_angles = np.delete(tilt_angles, indices)
            utils.set_tilt_angles(dataset, tilt_angles)
        except: # noqa
            # TODO what exception are we ignoring here?
            pass
