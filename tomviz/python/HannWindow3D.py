def transform_scalars(dataset):

    from tomviz import utils
    import numpy as np

    # Get the current volume as a numpy array.
    array = utils.get_array(dataset)

    # Create 3D hanning window.
    for axis, axis_size in enumerate(array.shape):
        # Set up shape for numpy broadcasting.
        filter_shape = [1, ] * array.ndim
        filter_shape[axis] = axis_size
        window = np.hanning(axis_size).reshape(filter_shape)
        array *= window

    # This is where you operate on your data
    result = array
    #result = window

    # This is where the transformed data is set, it will display in tomviz.
    utils.set_array(dataset, result)
