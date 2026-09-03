def transform(dataset, XRANGE=None, YRANGE=None, ZRANGE=None,
              fill_value=0.0):
    """Set every voxel inside the box to the fill value.

    Ranges are [start, end) voxel indices along each axis, as picked
    with the box widget. The volume keeps its shape, which makes this
    the complement of Crop.
    """
    import numpy as np

    array = dataset.active_scalars
    if array is None:
        raise RuntimeError('No scalars found!')
    if not (XRANGE and YRANGE and ZRANGE):
        raise RuntimeError('Select the region to clear first')

    result = np.copy(array)
    result[XRANGE[0]:XRANGE[1], YRANGE[0]:YRANGE[1], ZRANGE[0]:ZRANGE[1]] = \
        fill_value
    dataset.active_scalars = result
