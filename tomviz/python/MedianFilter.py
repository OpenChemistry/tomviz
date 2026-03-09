def transform(dataset, size=2):
    """Apply a Median filter to dataset."""
    """ Median filter is a nonlinear filter used to reduce noise."""

    import scipy.ndimage
    import numpy as np

    array = dataset.active_scalars

    # Transform the dataset.
    result = np.empty_like(array)
    scipy.ndimage.filters.median_filter(array, size, output=result)

    # Set the result as the new scalars.
    dataset.active_scalars = result
