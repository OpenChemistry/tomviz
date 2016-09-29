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

    # Generate Tilt Angles.
    angles = np.linspace(startAngle, startAngle + \
                         (Nproj - 1) * angleIncrement, Nproj);

    volume = utils.get_array(dataset)
    Ny = volume.shape[1];
    Nz = volume.shape[2];
    #calculate the size s.t. it contains the entire volume
    N = np.round(np.sqrt(Ny**2 + Nz**2))
    N = int(np.floor(N / 2.0) * 2 + 1) #make the size an odd integer

    #pad volume
    pad_y_pre = np.ceil((N - Ny) / 2.0)
    pad_y_post = np.floor((N - Ny) / 2.0)
    pad_z_pre = np.ceil((N - Nz) / 2.0)
    pad_z_post = np.floor((N - Nz) / 2.0)
    volume_pad = np.lib.pad(
        volume, ((0, 0), (pad_y_pre, pad_y_post), (pad_z_pre, pad_z_post)), 'constant')

    Nslice = volume.shape[0]; # Number of slices along rotation axis.
    tiltSeries = np.zeros((Nslice, N, Nproj))

    for i in range(Nproj):
        # Rotate volume about x-axis
        rotatedVolume = scipy.ndimage.interpolation.rotate(
            volume_pad, angles[i], axes=(1, 2), reshape=False, order=1)
        print rotatedVolume.shape
        # Calculate projection
        tiltSeries[:, :, i] = np.sum(rotatedVolume, axis=2)

    # Set the result as the new scalars.
    utils.set_array(dataset, tiltSeries)

    # Mark dataset as tilt series.
    utils.mark_as_tiltseries(dataset)

    # Save tilt angles.
    utils.set_tilt_angles(dataset, angles)
