def transform_scalars(dataset):
    """Apply a Median filter to dataset."""
    """ Median filter is a nonlinear filter used to reduce noise."""

    #----USER SPECIFIED VARIABLES-----#
    ###Size###    #Specify size of the Median filter
    #---------------------------------#

    from tomviz import utils
    import scipy.ndimage

    array = utils.get_array(dataset)

    # Transform the dataset.
    result = scipy.ndimage.filters.median_filter(array,size)

    # Set the result as the new scalars.
    utils.set_array(dataset, result)
