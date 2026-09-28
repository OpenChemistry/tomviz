from tomviz import utils


def transform(dataset, rotation_angle=90.0, rotation_axis=0, expand=True):

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

    # Multiples of 90 degrees need no interpolation: np.rot90 is exact
    # and orders of magnitude faster than ndimage.rotate. With expand
    # disabled the plane must be square, since rot90 always swaps the
    # plane's dimensions; otherwise fall through to ndimage, which
    # center-crops to the original shape.
    quarter_turns = rotation_angle / 90.0
    is_quarter = np.isclose(quarter_turns, np.round(quarter_turns))
    plane_square = data_py.shape[axes[0]] == data_py.shape[axes[1]]
    if is_quarter and (expand or plane_square):
        k = int(np.round(quarter_turns)) % 4
        # ndimage.rotate sorts a descending axes pair (without flipping
        # the angle); np.rot90 does not, so mirror that here to keep the
        # rotation direction identical. Verified against ndimage.rotate
        # for every axis and quarter turn.
        rot_axes = (int(axes[0]), int(axes[1]))
        if rot_axes[0] > rot_axes[1]:
            rot_axes = (rot_axes[1], rot_axes[0])
        if k != 0:
            rotated = np.rot90(data_py, k=k, axes=rot_axes)
            dataset.active_scalars = np.asfortranarray(rotated)
        return

    if expand:
        # The output box grows to hold the rotated volume, and the corners
        # it gains are zero. Even a few degrees adds a lot: rotating a
        # 512-wide volume by 3 degrees pads the perpendicular axis by
        # 512*sin(3) = 27 slices, whose leading ones hold only a thin
        # wedge of data and read as empty.
        shape = utils.rotate_shape(data_py, rotation_angle, axes=axes)
    else:
        shape = data_py.shape
    data_py_return = np.empty(shape, data_py.dtype, order='F')
    ndimage.rotate(data_py, rotation_angle, output=data_py_return, axes=axes,
                   reshape=expand)

    dataset.active_scalars = data_py_return
