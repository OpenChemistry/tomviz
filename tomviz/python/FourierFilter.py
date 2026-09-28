def transform(dataset, filter_type=0, cutoff=20.0, width=5.0,
              band_profile=0, dimensionality=0, slice_axis=2):
    """Filter the dataset in Fourier (reciprocal) space.

    The data is Fourier transformed, multiplied by a radially symmetric
    filter, and transformed back. Distances are measured in frequency
    bins (voxels) from the DC term of the centered (fftshift-ed)
    spectrum, so a cutoff of 20 keeps/removes everything within 20 bins
    of the center.
    """
    import numpy as np

    array = dataset.active_scalars
    if array is None:
        raise RuntimeError('No data array found!')

    # NaNs (e.g. from masked reconstructions) would poison the FFT
    array = np.nan_to_num(np.asarray(array, dtype=np.float64))

    if dimensionality == 0:
        result = _apply_filter(array, filter_type, cutoff, width,
                               band_profile)
    else:
        # Filter each 2D slice perpendicular to the chosen axis
        moved = np.moveaxis(array, slice_axis, 0)
        filtered = np.empty_like(moved)
        for i in range(moved.shape[0]):
            filtered[i] = _apply_filter(moved[i], filter_type, cutoff,
                                        width, band_profile)
        result = np.moveaxis(filtered, 0, slice_axis)

    dataset.active_scalars = np.asfortranarray(result.astype(np.float32))


def _frequency_distance(shape):
    # Distance of every frequency bin from the DC term of the
    # fftshift-ed spectrum, in bins. numpy's fftshift puts DC at n // 2.
    import numpy as np

    grids = np.meshgrid(*(np.arange(n, dtype=np.float64) - n // 2
                          for n in shape), indexing='ij')
    return np.sqrt(sum(g * g for g in grids))


def _filter_response(dist, filter_type, cutoff, width, band_profile):
    import numpy as np
    from scipy.special import erf

    if filter_type in (0, 1):
        # Soft sphere of radius `cutoff` with edge softness `width`
        # (hard edge when width <= 0); high-pass is its complement.
        if width > 0:
            keep = 0.5 * (erf((dist + cutoff) / (np.sqrt(2) * width)) -
                          erf((dist - cutoff) / (np.sqrt(2) * width)))
        else:
            keep = (dist <= cutoff).astype(np.float64)
        return keep if filter_type == 0 else 1.0 - keep

    # Band-pass: a shell centered at radius `cutoff` with width `width`
    offset = dist - cutoff
    if band_profile == 0:  # Gaussian
        return np.exp(-offset**2 / width**2)
    if band_profile == 1:  # Hann (single lobe peaking at the shell)
        response = np.cos(np.pi * offset / (2.0 * width))**2
        response[np.abs(offset) > width] = 0.0
        return response
    if band_profile == 2:  # Rectangular
        return (np.abs(offset) <= width).astype(np.float64)
    # Erf: soft rectangle with a one-bin edge
    return 0.5 * (erf(offset + width) - erf(offset - width))


def _apply_filter(array, filter_type, cutoff, width, band_profile):
    import numpy as np

    response = _filter_response(_frequency_distance(array.shape),
                                filter_type, cutoff, width, band_profile)
    spectrum = np.fft.fftshift(np.fft.fftn(array))
    return np.real(np.fft.ifftn(np.fft.ifftshift(spectrum * response)))
