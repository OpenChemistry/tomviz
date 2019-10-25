def transform(dataset):
    """
    For each tilt image, the method calculates its histogram
    and then chooses the highest peak as the background level and subtracts it
    from the image.
    It does NOT set negative pixels to zero.
    """

    import numpy as np

    data = dataset.active_scalars # Get data as numpy array.

    if data is None: #Check if data exists
        raise RuntimeError("No data array found!")

    data_bs = data.astype(np.float32) # Change tilt series type to float.

    for i in range(0, data.shape[2]):
        (hist, bins) = np.histogram(data[:, :, i].flatten(), 256)
        data_bs[:, :, i] = data_bs[:, :, i] - bins[np.argmax(hist)]

    dataset.active_scalars = data_bs
