def transform_scalars(dataset,  XRANGE=None, YRANGE=None, ZRANGE=None):
    """Define this method for Python operators that
    transform input scalars"""

    from tomviz import utils
    import numpy as np

    array = utils.get_array(dataset)
    if array is None:
        raise RuntimeError("No scalars found!")

    # Transform the dataset.
    result = np.copy(array)
    result[XRANGE[0]:XRANGE[1], YRANGE[0]:YRANGE[1], ZRANGE[0]:ZRANGE[1]] = 0
    # Set the result as the new scalars.
    utils.set_array(dataset, result)
