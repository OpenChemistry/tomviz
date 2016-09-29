def transform_scalars(dataset):
    '''Normalize tilt series so that each tilt image has the same total intensity.'''

    from tomviz import utils
    import numpy as np

    data = utils.get_array(dataset) # Get data as numpy array

    if data is None: # Check if data exists
        raise RuntimeError("No data array found!")

    data = data.astype(np.float32) # Change tilt series type to float.

    # Calculate average intensity of tilt series.
    intensity = np.sum(np.average(data, 2))

    for i in range(0, data.shape[2]):
        # Normalize each tilt image.
        data[:, :, i] = data[:, :, i] / np.sum(data[:, :, i]) * intensity

    utils.set_array(dataset, data)
