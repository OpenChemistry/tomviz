# Rotate a 3D dataset using SciPy Interpolation libraries.
#
# Developed as part of the tomviz project (www.tomviz.com).


def transform_scalars(dataset):

    #----USER SPECIFIED VARIABLES-----#
    ###ROT_AXIS###    #Specify Tilt Axis Dimensions x=0, y=1, z=2
    ###ROT_ANGLE###   #Rotate the dataset by an Angle (in degrees)
    #---------------------------------#

    from tomviz import utils
    import numpy as np
    from scipy import ndimage

    data_py = utils.get_array(dataset) #get data as numpy array

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")

    if ROT_AXIS == []: #If tilt axis is not given, assign one.
        # Find the smallest array dimension, assume it is the tilt angle axis.
        if data_py.ndim >= 2:
            ROT_AXIS = np.argmin(data_py.shape)
        else:
            raise RuntimeError("Data Array is not 2 or 3 dimensions!")

    if ROT_ANGLE == []: # If tilt axis is not given, assign it to 90 degrees.
        ROT_ANGLE = 90

    print('Rotating Dataset...')

    data_py_return = ndimage.interpolation.rotate(
        data_py, ROT_ANGLE, axes=((ROT_AXIS + 1) % 3, (ROT_AXIS + 2) % 3))

    utils.set_array(dataset, data_py_return)
    print('Rotation Complete')
