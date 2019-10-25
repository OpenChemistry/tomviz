def transform(dataset, sigma=2.0):
    """Apply a Gaussian filter to tilt images."""
    """Gaussian Filter blurs the image and reduces the noise and details."""

    import scipy.ndimage
    import numpy as np

    tiltSeries = dataset.active_scalars

    # Transform the dataset.
    result = np.empty_like(tiltSeries)
    scipy.ndimage.filters.gaussian_filter(
        tiltSeries, [sigma, sigma, 0], output=result)

    # Set the result as the new scalars.
    dataset.active_scalars = result
