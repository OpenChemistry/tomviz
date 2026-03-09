def transform(dataset, threshold=None):
    """Remove bad pixels in tilt series."""

    import scipy.ndimage
    import numpy as np

    tiltSeries = dataset.active_scalars.astype(np.float32)

    for i in range(tiltSeries.shape[2]):
        I = tiltSeries[:, :, i]
        I_pad = np.pad(I, (1, 1), 'edge')

        # calculate standard deviation in a 3 x 3 window
        averageI2 = scipy.ndimage.filters.uniform_filter(I_pad ** 2)
        averageI = scipy.ndimage.filters.uniform_filter(I_pad)
        std = np.sqrt(abs(averageI2 - averageI**2))[1:-1, 1:-1]

        medianI = scipy.ndimage.filters.median_filter(I_pad, 2)[1:-1, 1:-1]

        #identify bad pixels
        badPixelsMask = abs(I - medianI) > std * threshold

        I[badPixelsMask] = medianI[badPixelsMask]
        tiltSeries[:, :, i] = I

    # Set the result as the new scalars.
    dataset.active_scalars = tiltSeries
