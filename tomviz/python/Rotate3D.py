# Rotate a 3D dataset using SciPy Interpolation libraries.
#
# Developed as part of the tomviz project (www.tomviz.com).


def transform_scalars(dataset, rotation_angle=90.0, rotation_axis=0):

    from tomviz import utils
    import numpy as np
    from scipy import ndimage

    data_py = utils.get_array(dataset) #get data as numpy array

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")

    if rotation_axis == []: #If tilt axis is not given, assign one.
        # Find the smallest array dimension, assume it is the tilt angle axis.
        if data_py.ndim >= 2:
            rotation_axis = np.argmin(data_py.shape)
        else:
            raise RuntimeError("Data Array is not 2 or 3 dimensions!")

    if rotation_angle == []: # If tilt angle not given, assign it to 90 degrees.
        rotation_angle = 90

    print('Rotating Dataset...')

    axis1 = (rotation_axis + 1) % 3
    axis2 = (rotation_axis + 2) % 3
    data_py_return = ndimage.interpolation.rotate(
        data_py, rotation_angle, axes=(axis1, axis2))

    utils.set_array(dataset, data_py_return)
    print('Rotation Complete')
