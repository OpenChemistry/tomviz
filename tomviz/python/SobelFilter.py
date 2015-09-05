def transform_scalars(dataset):
    """Apply a Sobel filter to dataset."""
    """Sobel Filter hgihlights high intensity variations."""
    
    #----USER SPECIFIED VARIABLES-----#
    ###Filter_AXIS###    #Specify Axis along which to calculate
    #---------------------------------#

    from tomviz import utils
    import numpy as np
    import scipy.ndimage

    array = utils.get_array(dataset)

    # transform the dataset
    result = scipy.ndimage.filters.sobel(array,axis=-Filter_AXIS)
    
    # set the result as the new scalars.
    utils.set_array(dataset, result)
    