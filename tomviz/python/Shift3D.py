# Shift a 3D dataset using SciPy Interpolation libraries.
#
# Developed as part of the tomviz project (www.tomviz.com).


def transform_scalars(dataset, SHIFT=None):

    from tomviz import utils
    from scipy import ndimage
    import numpy as np

    data_py = utils.get_array(dataset) # Get data as numpy array.

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")


    data_py_return = np.empty_like(data_py)
    ndimage.interpolation.shift(data_py, SHIFT, order=0, output=data_py_return)

    utils.set_array(dataset, data_py_return)
