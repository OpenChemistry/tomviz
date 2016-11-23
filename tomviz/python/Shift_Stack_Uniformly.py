# Shift all data uniformly (it is a rolling shift).
#
# Developed as part of the tomviz project (www.tomviz.com).


def transform_scalars(dataset, shift=[0, 0, 0]):
    from tomviz import utils
    import numpy as np

    data_py = utils.get_array(dataset) # Get data as numpy array.

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")

    data_py = np.roll(data_py, shift[0], axis=0)
    data_py = np.roll(data_py, shift[1], axis=1)
    data_py = np.roll(data_py, shift[2], axis=2)

    utils.set_array(dataset, data_py)
    print('Data has been shifted uniformly.')
