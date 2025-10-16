from tomviz import utils


@utils.apply_to_each_array
def transform(dataset, rotation_angle=90.0, rotation_axis=0):

    import numpy as np
    from scipy import ndimage

    data_py = dataset.active_scalars #get data as numpy array

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

    axis1 = (rotation_axis + 1) % 3
    axis2 = (rotation_axis + 2) % 3
    axes = (axis1, axis2)
    shape = utils.rotate_shape(data_py, rotation_angle, axes=axes)
    data_py_return = np.empty(shape, data_py.dtype, order='F')
    ndimage.interpolation.rotate(
        data_py, rotation_angle, output=data_py_return, axes=axes)

    dataset.active_scalars = data_py_return
