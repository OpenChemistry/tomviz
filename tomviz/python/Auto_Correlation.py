def transform_scalars(dataset):
    """Apply auto correlation on a dataset."""
    
    from tomviz import utils
    from scipy.fftpack import fftn, ifftn, fftshift
    import numpy as np
    
    array = utils.get_array(dataset)
    dim = []
    dim = array.shape
    
    #Transform the dataset.
    result = np.empty_like(array)
    result = abs(fftshift(ifftn(fftn(array)*np.conjugate(fftn(array)))))
    result = result/(dim[0]*dim[1]*dim[2])
    
    #Set the result as the new scalars.
    utils.set_array(dataset, np.asfortranarray(result))
