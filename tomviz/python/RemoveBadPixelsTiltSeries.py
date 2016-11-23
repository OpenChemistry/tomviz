def transform_scalars(dataset, threshold=None):
    """Remove bad pixels in tilt series."""

    from tomviz import utils
    import scipy.ndimage
    import numpy as np

    tiltSeries = utils.get_array(dataset).astype(np.float32)

    for i in range(tiltSeries.shape[2]):
        I = tiltSeries[:, :, i]
        I_pad = np.lib.pad(I, (1, 1), 'edge')

        # calculate standard deviation in a 3 x 3 window
        averageI2 = scipy.ndimage.filters.uniform_filter(I_pad ** 2)
        averageI = scipy.ndimage.filters.uniform_filter(I_pad)
        std = np.sqrt(abs(averageI2 - averageI**2))[1:-1, 1:-1]

        medianI = scipy.ndimage.filters.median_filter(I_pad, 2)[1:-1, 1:-1]

        #identiy bad pixels
        badPixelsMask = abs(I - medianI) > std * threshold

        I[badPixelsMask] = medianI[badPixelsMask]
        tiltSeries[:, :, i] = I

    # Set the result as the new scalars.
    utils.set_array(dataset, tiltSeries)
