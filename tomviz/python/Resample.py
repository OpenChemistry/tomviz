def transform_scalars(dataset):
    """Resample dataset """
    
    from tomviz import utils
    import numpy as np
    import scipy.ndimage
    
    #----USER SPECIFIED VARIABLES-----#
    #resampingFactor  = [1,1,1]  #Specify the shifts (x,y,z) applied to data
    ###resampingFactor###
    #---------------------------------#
    array = utils.get_array(dataset)
    
    # transform the dataset
    result = scipy.ndimage.interpolation.zoom(array, resampingFactor)
    
    # set the result as the new scalars.
    utils.set_array(dataset, result)
