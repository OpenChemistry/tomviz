def transform_scalars(dataset):
    """Reinterpret a signed integral array type as its unsigned counterpart.
    This can be used when the bytes of a data array have been interpreted as a
    signed array when it should have been interpreted as an unsigned array."""

    from tomviz import utils
    import numpy as np
    scalars = utils.get_scalars(dataset)
    if scalars is None:
        raise RuntimeError("No scalars found!")

    dtype = scalars.dtype
    dtype = dtype.type

    typeMap = {
        np.int8: np.uint8,
        np.int16: np.uint16,
        np.int32: np.uint32
    }

    typeAddend = {
        np.int8: 128,
        np.int16: 32768,
        np.int32: 2147483648
    }

    if dtype not in typeMap:
        raise RuntimeError("Scalars are not int8, int16, or int32")

    newType = typeMap[dtype]
    addend = typeAddend[dtype]

    newScalars = scalars.astype(dtype=newType) + addend
    utils.set_scalars(dataset, newScalars)
