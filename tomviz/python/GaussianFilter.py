def transform_scalars(dataset, sigma=2.0):
    """Apply a Gaussian filter to volume dataset."""
    """Gaussian Filter blurs the image and reduces the noise and details."""

    from tomviz import utils
    import scipy.ndimage
    import numpy as np

    array = utils.get_array(dataset)

    # Transform the dataset.
    result = np.empty_like(array)
    scipy.ndimage.filters.gaussian_filter(array, sigma, output=result)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
