# Perform alignment to the estimated rotation axis
#
# Developed as part of the tomviz project (www.tomviz.com).


def transform_scalars(dataset, SHIFT=None, rotation_angle=90.0, tilt_axis=0):
    from tomviz import utils
    from scipy import ndimage
    import numpy as np

    data_py = utils.get_array(dataset) # Get data as numpy array.

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")
    if SHIFT is None:
        SHIFT = np.zeros(len(data_py.shape), dtype=np.int)
    data_py_return = np.empty_like(data_py)
    ndimage.interpolation.shift(data_py, SHIFT, order=0, output=data_py_return)

    rotation_axis = 2 # This operator always assumes the rotation axis is Z
    if rotation_angle == []: # If tilt angle not given, assign it to 90 degrees.
        rotation_angle = 90

    axis1 = (rotation_axis + 1) % 3
    axis2 = (rotation_axis + 2) % 3
    axes = (axis1, axis2)
    shape = utils.rotate_shape(data_py_return, rotation_angle, axes=axes)
    data_py_return2 = np.empty(shape, data_py_return.dtype, order='F')
    ndimage.interpolation.rotate(
        data_py_return, rotation_angle, output=data_py_return2, axes=axes)

    utils.set_array(dataset, data_py_return2)
