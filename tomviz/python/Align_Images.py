# Align a tomography tilt series using cross correlation
#
# Developed as part of the tomviz project (www.tomviz.com)

def transform_scalars(dataset):

    #----USER SPECIFIED VARIABLES-----#
    TILT_AXIS  = []  #Specify Tilt Axis Dimensions x=0, y=1, z=2
    #---------------------------------#

    from tomviz import utils
    import numpy as np

    data_py = utils.get_array(dataset) #get data as numpy array

    if data_py is None: # Check if data exists
        raise RuntimeError("No data array found!")

    if TILT_AXIS == []: # If tilt axis is not given, find it
    # Find the smallest array dimension, assume it is the tilt angle axis.
        if data_py.ndim == 3:
            TILT_AXIS = np.argmin( data_py.shape )
        elif data_py.ndim == 2:
            raise RuntimeError("Data Array is 2 dimensions, it should be 3!")
        else:
            raise RuntimeError("Data Array is not 2 or 3 dimensions!")

    print('Aligning Images by Cross Correlation')
    for i in range(1,np.size(data_py,TILT_AXIS)): # Align image to previous
        if TILT_AXIS == 2:
            im0 = np.fft.fft2(data_py[:,:,i-1])
            im1 = np.fft.fft2(data_py[:,:,i])
            xcor = abs(np.fft.ifft2((im0 * im1.conjugate())))
            shift = np.unravel_index(xcor.argmax(), xcor.shape)
            print( shift )
            data_py[:,:,i] = np.roll( data_py[:,:,i], shift[0], axis = 0)
            data_py[:,:,i] = np.roll( data_py[:,:,i], shift[1], axis = 1)
        elif TILT_AXIS == 1:
            im0 = np.fft.fft2(data_py[:,i-1,:])
            im1 = np.fft.fft2(data_py[:,i,:])
            xcor = abs(np.fft.ifft2((im0 * im1.conjugate())))
            print( np.amax(xcor) )
            shift = np.unravel_index(xcor.argmax(), xcor.shape)
            print( shift )
            data_py[:,i,:] = np.roll( data_py[:,i,:], shift[0], axis = 0)
            data_py[:,i,:] = np.roll( data_py[:,i,:], shift[2], axis = 2)
        elif TILT_AXIS == 0:
            im0 = np.fft.fft2(data_py[i-1,:,:])
            im1 = np.fft.fft2(data_py[i,:,:])
            xcor = abs(np.fft.ifft2((im0 * im1.conjugate())))
            print( np.amax(xcor) )
            shift = np.unravel_index(xcor.argmax(), xcor.shape)
            print( shift )
            data_py[i,:,:] = np.roll( data_py[i,:,:], shift[1], axis = 1)
            data_py[i,:,:] = np.roll( data_py[i,:,:], shift[2], axis = 2)
        else:
            raise RuntimeError("Python Transform Error: Unknown TILT_AXIS.")

    utils.set_array(dataset, data_py)
    print('Align Images Complete')
