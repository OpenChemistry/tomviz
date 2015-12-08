def transform_scalars(dataset):
    from tomviz import utils
    import numpy as np

    data = utils.get_array(dataset) #get data as numpy array

    if data is None: #Check if data exists
        raise RuntimeError("No data array found!")

    data_bs = data.astype(np.float32) #change tilt series type to float

    for i in range(0,data.shape[2]):
        (hist,bins) = np.histogram(data[:,:,i].flatten(), 256)
        data_bs[:,:,i] = data_bs[:,:,i] - bins[np.argmax(hist)]

    utils.set_array(dataset, data_bs)