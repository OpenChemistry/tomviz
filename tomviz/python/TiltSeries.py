def transform_scalars(dataset):
    """Generate Tilt Series from Volume"""
    
    from tomviz import utils
    import numpy as np
    import scipy.ndimage
    
    
    #----USER SPECIFIED VARIABLES-----#
    ###startAngle###    #Starting angle
    ###angleIncrement###   #Angle increment
    ###Nproj### #Number of tilts
    #---------------------------------#
    
    # Generate Tilt Angles
    angles = np.linspace(startAngle,startAngle+(Nproj-1)*angleIncrement,Nproj);
    
    array = utils.get_array(dataset)
    N = array.shape[0];
    Nslice = array.shape[2]; #Number of slices along rotation axis
    tiltSeries = np.zeros((Nslice, N ,Nproj))
    for i in range(Nproj):
        #Rotate volume
        rotatedArray = scipy.ndimage.interpolation.rotate(array, angles[i],axes=(0,1),reshape=False)
        #Calculate Projection
        tiltSeries[:,:,i] = np.sum(rotatedArray,axis=0).transpose()

    # set the result as the new scalars.
    utils.set_array(dataset, tiltSeries)

    # Mark dataset as tilt series
    utils.mark_as_tiltseries(dataset)

    # Save tilt angles
    utils.set_tilt_angles(dataset, angles)