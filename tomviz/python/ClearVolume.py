from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset,  XRANGE=None, YRANGE=None, ZRANGE=None):
    """Define this method for Python operators that
    transform input scalars"""

    import numpy as np

    array = dataset.active_scalars
    if array is None:
        raise RuntimeError("No scalars found!")

    # Transform the dataset.
    result = np.copy(array)
    result[XRANGE[0]:XRANGE[1], YRANGE[0]:YRANGE[1], ZRANGE[0]:ZRANGE[1]] = 0
    # Set the result as the new scalars.
    dataset.active_scalars = result
