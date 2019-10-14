def transform(dataset, firstSlice=None, lastSlice=None, axis=2):
    """Delete Slices in Dataset"""

    import numpy as np

    # Get the current dataset.
    array = dataset.active_scalars

    # Get indices of the slices to be deleted.
    indices = np.linspace(firstSlice, lastSlice,
                          lastSlice - firstSlice + 1).astype(int)

    # Delete the specified slices.
    array = np.delete(array, indices, axis)

    # Set the result as the new scalars.
    dataset.active_scalars = array

    # Delete corresponding tilt anlges if dataset is a tilt series.
    if axis == 2:
        try:
            tilt_angles = dataset.tilt_angles
            tilt_angles = np.delete(tilt_angles, indices)
            dataset.tilt_angles = tilt_angles
        except: # noqa
            # TODO what exception are we ignoring here?
            pass
