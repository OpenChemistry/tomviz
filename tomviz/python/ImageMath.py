def transform(dataset, second_dataset, operation=0, normalize_first=False,
              resample_to_match=False):
    """Combine two datasets arithmetically, voxel by voxel.

    The primary dataset's active array is combined with the second
    dataset's active array (subtract, add, multiply, or divide) and the
    result replaces the primary's active array. With normalization on,
    each array is first divided by its own mean intensity, so datasets
    on different intensity scales (e.g. two elements, or two
    reconstructions) can be compared directly. With resampling on, a
    second dataset of a different shape is stretched onto this
    dataset's voxel grid first (linear interpolation, assuming both
    cover the same field of view), so a half-resolution XRF map can be
    combined with a ptychography reconstruction.
    """
    import numpy as np

    primary = dataset.active_scalars
    secondary = second_dataset.active_scalars

    if primary is None or secondary is None:
        raise RuntimeError('Both datasets must have an active scalar array')

    a = np.asarray(primary, dtype=np.float64)
    b = np.asarray(secondary, dtype=np.float64)

    if a.shape != b.shape:
        if not resample_to_match:
            raise RuntimeError(
                'Datasets must have the same shape: '
                f'{tuple(a.shape)} vs {tuple(b.shape)}. Enable '
                '"Resample To Match" to stretch the second dataset onto '
                'this one\'s grid.')
        b = _resample_to_shape(b, a.shape)

    if normalize_first:
        a_mean = a.mean()
        b_mean = b.mean()
        if a_mean == 0 or b_mean == 0:
            raise RuntimeError(
                'Cannot normalize: one of the datasets has zero mean')
        a = a / a_mean
        b = b / b_mean

    if operation == 0:
        result = a - b
    elif operation == 1:
        result = a + b
    elif operation == 2:
        result = a * b
    else:
        # Divide: voxels where the divisor is zero become zero rather
        # than inf/nan, so the result stays renderable.
        result = np.divide(a, b, out=np.zeros_like(a), where=b != 0)

    dataset.active_scalars = np.asfortranarray(result.astype(np.float32))


def _resample_to_shape(array, shape):
    """Stretch *array* onto *shape* with linear interpolation."""
    import numpy as np
    from scipy.ndimage import zoom

    # An axis that differs by only a voxel or two (an extra row or
    # column in one acquisition) is trimmed or padded rather than
    # stretched, which would blur it for no gain.
    factors = [1.0 if abs(n - m) <= 2 else n / m
               for n, m in zip(shape, array.shape)]
    out = array if all(f == 1.0 for f in factors) else \
        zoom(array, factors, order=1)
    # zoom rounds its output size; trim or edge-pad the odd voxel
    slices = tuple(slice(0, n) for n in shape)
    out = out[slices]
    pad = [(0, n - m) for n, m in zip(shape, out.shape)]
    if any(p[1] for p in pad):
        out = np.pad(out, pad, mode='edge')
    return out
