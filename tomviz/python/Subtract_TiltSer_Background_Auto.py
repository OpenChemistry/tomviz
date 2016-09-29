def transform_scalars(dataset):
    '''For each tilt image, the method calculates its histogram 
      and then chooses the highest peak as the background level and subtracts it from the image.'''
    '''It does NOT set negative pixels to zero.'''

    from tomviz import utils
    import numpy as np

    data = utils.get_array(dataset) # Get data as numpy array.

    if data is None: #Check if data exists
        raise RuntimeError("No data array found!")

    data_bs = data.astype(np.float32) # Change tilt series type to float.

    for i in range(0, data.shape[2]):
        (hist, bins) = np.histogram(data[:, :, i].flatten(), 256)
        data_bs[:, :, i] = data_bs[:, :, i] - bins[np.argmax(hist)]

    utils.set_array(dataset, data_bs)
