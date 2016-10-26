def transform_scalars(dataset):
    """Remove bad pixels in tilt series."""

    #----USER SPECIFIED VARIABLES-----#
    ###s###
    #---------------------------------#

    from tomviz import utils
    import scipy.ndimage
    import numpy as np
    
    tiltSeries = utils.get_array(dataset)
    
    for i in range(tiltSeries.shape[2]):
        tiltImage = tiltSeries[:, :, i]
        medianFilteredImage = scipy.ndimage.filters.median_filter(tiltImage, 2)
        differenceImage = tiltImage - medianFilteredImage
        sigma = np.std(differenceImage.flatten())
        badPixelsMask = abs(tiltSeries[:, :, i]) > s * sigma
        tiltImage[badPixelsMask] = medianFilteredImage[badPixelsMask]
        tiltSeries[:, :, i] = tiltImage

    # Set the result as the new scalars.
    utils.set_array(dataset, tiltSeries)
