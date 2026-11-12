def transform(dataset, threshold=None):
    """Remove bad pixels in tilt series."""

    import scipy.ndimage
    import numpy as np

    tiltSeries = dataset.active_scalars.astype(np.float32)

    for i in range(tiltSeries.shape[2]):
        I = tiltSeries[:, :, i]
        # Mirror the edge, so a pixel there doesn't count itself as its
        # own neighbor
        I_pad = np.pad(I, (1, 1), 'reflect')

        # Standard deviation of the 8 neighbors in a 3 x 3 window, leaving
        # out the pixel itself, which would inflate it when bad. The
        # neighbor farthest from their mean is left out too: a second bad
        # pixel next to this one would otherwise inflate it as much, and
        # both would be missed. Scaled to match the plain 8-neighbor value
        # on noise, so detail and edges still raise it as before
        averageI2 = (9 * scipy.ndimage.uniform_filter(I_pad ** 2) -
                     I_pad ** 2) / 8
        averageI = (9 * scipy.ndimage.uniform_filter(I_pad) - I_pad) / 8
        # The neighbor farthest from their mean is the largest or the
        # smallest of them
        ring = np.ones((3, 3), bool)
        ring[1, 1] = False
        largest = scipy.ndimage.maximum_filter(I_pad, footprint=ring)
        smallest = scipy.ndimage.minimum_filter(I_pad, footprint=ring)
        farthest = np.where(largest - averageI > averageI - smallest,
                            largest, smallest)
        averageI = (8 * averageI - farthest) / 7
        averageI2 = (8 * averageI2 - farthest ** 2) / 7
        std = 1.301 * np.sqrt(abs(averageI2 - averageI**2))[1:-1, 1:-1]
        # Neighbors that agree (flat, quantized or sparse regions) have no
        # deviation, so floor it at the typical nonzero one
        nonzero = std[std > 0]
        if nonzero.size:
            std = np.maximum(std, np.median(nonzero))

        # Median of the 3 x 3 window centered on the pixel
        medianI = scipy.ndimage.median_filter(I_pad, 3)[1:-1, 1:-1]

        # Identify bad pixels. When the neighbor left out stands out too,
        # it is either a second bad pixel, caught as well, or the rest of
        # a thin line, which is kept
        badPixelsMask = abs(I - medianI) > std * threshold
        alone = abs(farthest[1:-1, 1:-1] - medianI) <= std * threshold
        badPixelsMask &= alone | scipy.ndimage.binary_dilation(
            badPixelsMask, ring)

        I[badPixelsMask] = medianI[badPixelsMask]
        tiltSeries[:, :, i] = I

    # Set the result as the new scalars.
    dataset.active_scalars = tiltSeries
