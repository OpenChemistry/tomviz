def transform_scalars(dataset):

    from tomviz import utils
    import numpy as np

    # Get the current volume as a numpy array.
    array = utils.get_array(dataset)

    # Save the type
    input_dtype = array.dtype
    array = array.astype(np.float32)

    # Create 3D hanning window.
    for axis, axis_size in enumerate(array.shape):
        # Set up shape for numpy broadcasting.
        filter_shape = [1, ] * array.ndim
        filter_shape[axis] = axis_size
        window = np.hanning(axis_size).reshape(filter_shape)
        array *= window

    # Recast the data to the input type
    result = array.astype(input_dtype)
    utils.set_array(dataset, result)
