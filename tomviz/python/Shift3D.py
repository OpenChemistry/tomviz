# Shift a 3D dataset using SciPy Interpolation libraries.
#
# Developed as part of the tomviz project (www.tomviz.com).
from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset, SHIFT=None):

    from scipy import ndimage
    import numpy as np

    data_py = dataset.active_scalars # Get data as numpy array.

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")

    data_py_return = np.empty_like(data_py)
    ndimage.interpolation.shift(data_py, SHIFT, order=0, output=data_py_return)

    dataset.active_scalars = data_py_return
