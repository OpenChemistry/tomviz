def transform_scalars(dataset):
    '''Normalize tilt series so that each tilt image has the same total intensity.'''
    
    from tomviz import utils
    import numpy as np

    data = utils.get_array(dataset) #get data as numpy array

    if data is None: #Check if data exists
        raise RuntimeError("No data array found!")

    data = data.astype(np.float32) #change tilt series type to float
  
    intensity = np.sum(np.average(data,2)) #calculate average intensity of tilt series

    for i in range(0,data.shape[2]):
        data[:,:,i] = data[:,:,i]/np.sum(data[:,:,i])*intensity #normalize each tilt image

    utils.set_array(dataset, data)