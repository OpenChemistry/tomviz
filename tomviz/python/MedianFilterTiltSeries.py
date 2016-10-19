def transform_scalars(dataset):
    """Apply a Median filter to dataset."""
    """ Median filter is a nonlinear filter used to reduce noise."""

    #----USER SPECIFIED VARIABLES-----#
    ###size###    #Specify size of the Median filter
    #---------------------------------#

    from tomviz import utils
    import scipy.ndimage

    tiltSeries = utils.get_array(dataset)

    # Transform the dataset.
    for i in range(tiltSeries.shape[2]):
      tiltSeries[:,:,i] = scipy.ndimage.filters.median_filter(tiltSeries[:,:,i],
                                                              size)

    # Set the result as the new scalars.
    utils.set_array(dataset, tiltSeries)
