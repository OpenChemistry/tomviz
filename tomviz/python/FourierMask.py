def transform(dataset, mask, edge_softness=2.0, include_friedel_mates=True,
              invert=False):
    """Keep the parts of the spectrum selected by a mask volume.

    The mask is any volume of the same shape as this dataset whose
    non-zero voxels mark the reciprocal-space regions to keep, typically
    a Binary Threshold or Connected Components result computed on a
    "Fast Fourier Transform (FFT)" branch of the data. The mask edges
    are softened with a Gaussian so the sharp cut does not ring in real
    space, the
    dataset's spectrum is multiplied by it, and the real part of the
    inverse transform replaces the data.
    """
    import numpy as np
    from scipy.ndimage import gaussian_filter

    array = dataset.active_scalars
    mask_array = mask.active_scalars
    if array is None or mask_array is None:
        raise RuntimeError('Both the dataset and the mask need an array')

    array = np.nan_to_num(np.asarray(array, dtype=np.float64))
    window = (np.asarray(mask_array) != 0).astype(np.float64)
    if window.shape != array.shape:
        raise RuntimeError(
            f'The mask shape {tuple(window.shape)} does not match the data '
            f'shape {tuple(array.shape)}. Compute the mask on an '
            '"Fast Fourier Transform (FFT)" branch of this dataset.')
    if not window.any():
        raise RuntimeError('The mask has no non-zero voxels')

    # The spectrum of real data is Hermitian: every peak has a mate
    # mirrored through the DC bin (n // 2 after fftshift), so mirror the
    # mask the same way.
    if include_friedel_mates:
        mirrored = np.flip(window)
        for axis, n in enumerate(window.shape):
            if n % 2 == 0:
                mirrored = np.roll(mirrored, 1, axis=axis)
        window = np.maximum(window, mirrored)

    if edge_softness > 0:
        window = gaussian_filter(window, edge_softness, mode='nearest')
        np.clip(window, 0.0, 1.0, out=window)
    if invert:
        window = 1.0 - window

    spectrum = np.fft.fftshift(np.fft.fftn(array))
    result = np.real(np.fft.ifftn(np.fft.ifftshift(spectrum * window)))
    dataset.active_scalars = np.asfortranarray(result.astype(np.float32))
