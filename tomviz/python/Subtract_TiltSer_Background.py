def transform(dataset, XRANGE=None, YRANGE=None, ZRANGE=None):
    '''For each tilt image, the method uses average pixel value of selected region
      as the background level and subtracts it from the image.'''
    '''It does NOT set negative pixels to zero.'''

    import numpy as np

    data_bs = dataset.active_scalars # Get data as numpy array.

    data_bs = data_bs.astype(np.float32) # Change tilt series type to float.

    if data_bs is None: #Check if data exists
        raise RuntimeError("No data array found!")

    for i in range(ZRANGE[0], ZRANGE[1]):
        a = data_bs[:, :, i] - \
            np.average(data_bs[XRANGE[0]:XRANGE[1], YRANGE[0]:YRANGE[1], i])
        data_bs[:, :, i] = a

    dataset.active_scalars = data_bs
