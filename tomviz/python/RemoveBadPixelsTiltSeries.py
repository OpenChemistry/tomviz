def transform(dataset, threshold=None):
    """Remove bad pixels in tilt series."""

    import scipy.ndimage
    import numpy as np

    tiltSeries = dataset.active_scalars.astype(np.float32)

    for i in range(tiltSeries.shape[2]):
        I = tiltSeries[:, :, i]
        I_pad = np.pad(I, (1, 1), 'edge')

        # Standard deviation of the 8 neighbors in a 3 x 3 window. The
        # pixel itself is left out, since a bad pixel would otherwise
        # inflate the deviation it is measured against (a lone spike
        # could never exceed about 3.2 of them).
        averageI2 = (9 * scipy.ndimage.uniform_filter(I_pad ** 2) -
                     I_pad ** 2) / 8
        averageI = (9 * scipy.ndimage.uniform_filter(I_pad) - I_pad) / 8
        std = np.sqrt(abs(averageI2 - averageI**2))[1:-1, 1:-1]

        medianI = scipy.ndimage.median_filter(I_pad, 2)[1:-1, 1:-1]

        #identify bad pixels
        badPixelsMask = abs(I - medianI) > std * threshold

        I[badPixelsMask] = medianI[badPixelsMask]
        tiltSeries[:, :, i] = I

    # Set the result as the new scalars.
    dataset.active_scalars = tiltSeries
