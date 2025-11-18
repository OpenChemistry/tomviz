# Shift all data uniformly (it is a rolling shift).
#
# Developed as part of the tomviz project (www.tomviz.com).

from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset, shift=[0, 0, 0]):
    import numpy as np

    data_py = dataset.active_scalars # Get data as numpy array.

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")

    data_py[:] = np.roll(data_py, shift[0], axis=0)
    data_py[:] = np.roll(data_py, shift[1], axis=1)
    data_py[:] = np.roll(data_py, shift[2], axis=2)

    # Data was modified in place
    print('Data has been shifted uniformly.')
