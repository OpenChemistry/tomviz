from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset, pad_size_before=[0, 0, 0], pad_size_after=[0, 0, 0],
              pad_mode_index=0):
    """Pad dataset"""
    import numpy as np

    padModes = ['constant', 'edge', 'wrap', 'minimum', 'median']
    padMode = padModes[pad_mode_index]
    array = dataset.active_scalars #get data as numpy array

    if array is None: #Check if data exists
        raise RuntimeError("No data array found!")

    padWidthX = (pad_size_before[0], pad_size_after[0])
    padWidthY = (pad_size_before[1], pad_size_after[1])
    padWidthZ = (pad_size_before[2], pad_size_after[2])

    pad_width = (padWidthX, padWidthY, padWidthZ)

    result_shape = np.zeros(3, dtype=int)
    result_shape[0] = array.shape[0] + pad_size_before[0] + pad_size_after[0]
    result_shape[1] = array.shape[1] + pad_size_before[1] + pad_size_after[1]
    result_shape[2] = array.shape[2] + pad_size_before[2] + pad_size_after[2]

    result = np.empty(result_shape, array.dtype, order='F')

    # pad the data.
    result[:] = np.pad(array, pad_width, padMode)

    dataset.active_scalars = result

    # If dataset is marked as tilt series, update tilt angles
    if padWidthZ[0] + padWidthZ[1] > 0:
        try:
            tilt_angles = dataset.tilt_angles
            tilt_angles = np.pad(tilt_angles, padWidthZ, padMode)
            dataset.tilt_angles = tilt_angles
        except: # noqa
            # TODO What exception are we ignoring?
            pass
