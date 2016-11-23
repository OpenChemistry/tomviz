def transform_scalars(dataset, sigma=2.0):
    """Apply a Gaussian filter to tilt images."""
    """Gaussian Filter blurs the image and reduces the noise and details."""

    from tomviz import utils
    import scipy.ndimage

    tiltSeries = utils.get_array(dataset)

    # Transform the dataset.
    result = scipy.ndimage.filters.gaussian_filter(
        tiltSeries, [sigma, sigma, 0])

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
