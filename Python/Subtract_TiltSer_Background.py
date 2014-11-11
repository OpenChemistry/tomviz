#Subtract Background from a tilt series.
#
#developed as part of the tomviz project (www.tomviz.com)

def transform_scalars(dataset):

    #----USER SPECIFIED VARIABLES-----#
    BKGRD_WIN_START = [0,0]  #Specify the background window
    BKGRD_WIN_END   = [0,0]  #Specify the background window
    TILT_AXIS  = []  #Specify Tilt Axis
    #---------------------------------#
        
    from tomviz import utils
    import numpy as np
    
    data_py = utils.get_array(dataset) #get data as numpy array
    
    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")    
    
    if TILT_AXIS == []: #If tilt axis is not given, find it
    #Find smallest array dimension, assume it is the tilt angle axis
        if data_py.ndim == 3:
            TILT_AXIS = np.argmin( data_py.shape )
        elif data_py.ndim == 2:
            raise RuntimeError("Data Array is 2 dimensions, it should be 3!")
        else: 
            raise RuntimeError("Data Array is not 2 or 3 dimensions!")
            
    if np.any(np.array(BKGRD_WIN_END) - np.array(BKGRD_WIN_START)) <= 0:
        raise RuntimeError("Background Window Domain is Zero or Negative!")
        
    for i in range(0,np.size(data_py,TILT_AXIS)-1):
        if TILT_AXIS == 2:
            bkgrd = np.mean(data_py[BKGRD_WIN_START[0]:BKGRD_WIN_END[0],BKGRD_WIN_START[1]:BKGRD_WIN_END[1],i])
            data_py[:,:,i] = data_py[:,:,i] - bkgrd
        elif TILT_AXIS == 1:
            bkgrd = np.mean(data_py[BKGRD_WIN_START[0]:BKGRD_WIN_END[0],i,BKGRD_WIN_START[1]:BKGRD_WIN_END[1]])
            data_py[:,i,:] = data_py[:,i,:] - bkgrd
        elif TILT_AXIS == 0:
            bkgrd = np.mean(i,data_py[BKGRD_WIN_START[0]:BKGRD_WIN_END[0],BKGRD_WIN_START[1]:BKGRD_WIN_END[1]])
            data_py[i,:,:] = data_py[i,:,:] - bkgrd
        else:
            raise RuntimeError("Python Transform Error: Unknown TILT_AXIS.")
    
    utils.set_array(dataset, data_py)
    print('Background Subtract Tilt Series Images Complete')