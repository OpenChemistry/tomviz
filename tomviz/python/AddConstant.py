def transform_scalars(dataset, constant=0):
    """Add a constant to the data set"""

    from tomviz import utils
    import numpy as np

    scalars = utils.get_scalars(dataset)
    if scalars is None:
        raise RuntimeError("No scalars found!")

    # Try to be a little smart so that we don't always just produce a
    # double-precision output
    newMin = np.min(scalars) + constant
    newMax = np.max(scalars) + constant
    if (constant).is_integer() and newMin.is_integer() and newMax.is_integer():
        # Let ints be ints!
        constant = int(constant)
        newMin = int(newMin)
        newMax = int(newMax)
    for dtype in [np.uint8, np.int8, np.uint16, np.int16, np.uint32, np.int32,
                  np.uint64, np.int64, np.float32, np.float64]:
        if np.can_cast(newMin, dtype) and np.can_cast(newMax, dtype):
            constant = np.array([constant], dtype=dtype)
            break

    # numpy should cast to an appropriate output type to avoid overflow
    result = scalars + constant

    utils.set_scalars(dataset, result)
