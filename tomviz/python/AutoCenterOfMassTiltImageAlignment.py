def transform_scalars(dataset):
    """Automatically align tilt images by center of mass method"""

    from tomviz import utils
    import numpy as np
    
    #----USER SPECIFIED VARIABLES-----#
    ###XRANGE###
    ###YRANGE###
    ###ZRANGE###
    #---------------------------------#

    tiltSeries = utils.get_array(dataset)  # get data as numpy array

    for i in range(1, np.size(tiltSeries, 2)):
        im0 = np.fft.fft2(tiltSeries[:, :, i - 1])
        im1 = np.fft.fft2(tiltSeries[:, :, i])
        xcor = abs(np.fft.ifft2((im0 * im1.conjugate())))
        shifts = np.unravel_index(xcor.argmax(), xcor.shape)
        tiltSeries[:, :, i] = np.roll(tiltSeries[:, :, i], shifts[0], axis=0)
        tiltSeries[:, :, i] = np.roll(tiltSeries[:, :, i], shifts[1], axis=1)

    utils.set_array(dataset, tiltSeries)
    print('Align Images Complete')
