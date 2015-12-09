def transform_scalars(dataset):
    """Calculate gradient magnitude of each tilt image using Sobel operator"""
    
    from tomviz import utils
    import numpy as np
    import scipy.ndimage.filters
    
    array = utils.get_array(dataset)
    array = array.astype(np.float32)
    
    # transform the dataset along the third axis
    aaSobelX = scipy.ndimage.filters.sobel(array,axis=0) #1D X-axis sobel
    aaSobelY = scipy.ndimage.filters.sobel(array,axis=1) #1D Y-axis sobel
    
    result = np.hypot(aaSobelX,aaSobelY) #calculate 2D sobel for each image slice
    
    # set the result as the new scalars.
    utils.set_array(dataset, result)
