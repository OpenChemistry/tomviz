def transform_scalars(dataset):
    """Apply a Median filter to dataset."""
    """ Median filter is a nonlinear filter used to reduce noise."""
    
    #----USER SPECIFIED VARIABLES-----#
    ###Size###    #Specify size of the Median filter
    #---------------------------------#

    from tomviz import utils
    import numpy as np
    import scipy.ndimage

    array = utils.get_array(dataset)

    # transform the dataset
    result = scipy.ndimage.filters.median_filter(array,sigma)
    
    # set the result as the new scalars.
    utils.set_array(dataset, result)
    
