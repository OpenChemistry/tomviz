def transform(dataset):
    """Calculate gradient magnitude of each tilt image using Sobel operator"""

    import numpy as np
    import scipy.ndimage.filters

    array = dataset.active_scalars
    array = array.astype(np.float32)

    # Transform the dataset along the third axis.
    aaSobelX = np.empty_like(array)
    # 1D X-axis sobel
    scipy.ndimage.filters.sobel(array, axis=0, output=aaSobelX)
    aaSobelY = np.empty_like(array)
    # 1D Y-axis sobel
    scipy.ndimage.filters.sobel(array, axis=1, output=aaSobelY)

    # Calculate 2D sobel for each image slice.
    result = np.hypot(aaSobelX, aaSobelY)

    # Set the result as the new scalars.
    dataset.active_scalars = result
