def transform_scalars(dataset):
    """Apply a 2D Sobel filter to tilt series"""
    """Sobel filter highlights high intensity variations and edges"""
    
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
