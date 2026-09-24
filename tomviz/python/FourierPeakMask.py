import tomviz.nodes


class FourierPeakMask(tomviz.nodes.TransformNode):

    def transform(self, inputs, auto_detect=True, threshold=0.65,
                  exclude_center_radius=8.0, min_peak_size=3, max_peaks=20,
                  centers='', centers_file='', radius=15.0, sigma=5.0,
                  include_friedel_mates=True):
        """Keep only the reciprocal-space volumes around the given peaks.

        The dataset is Fourier transformed, multiplied by soft spherical
        windows centered on each peak (plus, optionally, their Friedel
        mates mirrored through the spectrum center), and transformed
        back; the real part is kept. This is the equivalent of
        selected-area electron diffraction: only the periodicities
        represented by the chosen peaks survive.

        With auto-detection on, the peaks are found by thresholding the
        normalized log magnitude of the spectrum (the same quantity the
        "Fast Fourier Transform (FFT)" operator displays), labeling the
        connected regions above the threshold and taking the strongest
        voxel of
        each, skipping the central (DC) region and specks smaller than
        min_peak_size. The peaks found are written back into the Peak
        Centers parameter so they can be reviewed or edited.

        Otherwise peak coordinates are voxel indices into the centered
        (fftshift-ed) spectrum, exactly as displayed by "Fast Fourier
        Transform (FFT)": find each peak's (x, y, z) index there and enter
        one peak per
        line (or semicolon) as "x, y, z". A CSV file with one x,y,z row
        per peak (extra columns and header lines are ignored) can be
        used instead.
        """
        import numpy as np
        from scipy.special import erf

        dataset = inputs['volume']
        array = dataset.active_scalars
        if array is None:
            raise RuntimeError('No data array found!')

        array = np.nan_to_num(np.asarray(array, dtype=np.float64))
        shape = array.shape
        dc = [n // 2 for n in shape]

        self.progress.maximum = 4
        self.progress.message = 'Fourier transform'
        spectrum = np.fft.fftshift(np.fft.fftn(array))
        self.progress.value = 1

        if auto_detect:
            self.progress.message = 'Finding peaks'
            peaks = _detect_peaks(np.abs(spectrum), dc, threshold,
                                  exclude_center_radius, min_peak_size,
                                  max_peaks, include_friedel_mates)
            if not peaks:
                raise RuntimeError(
                    'No peaks found. Lower the threshold, or reduce the '
                    'excluded center radius and minimum peak size.')
            self.set_parameter('centers', '; '.join(
                ', '.join(str(int(c)) for c in peak) for peak in peaks))
        else:
            peaks = _parse_centers(centers, centers_file)
            if not peaks:
                raise RuntimeError(
                    'No peak centers given. Enter one "x, y, z" per line, '
                    'or select a CSV file with one x,y,z row per peak.')
            for peak in peaks:
                if len(peak) != len(shape):
                    raise RuntimeError(f'Peak {peak} does not have '
                                       f'{len(shape)} coordinates')
                if any(c < 0 or c >= n for c, n in zip(peak, shape)):
                    raise RuntimeError(f'Peak {peak} is outside the data '
                                       f'extents {tuple(shape)}')
        self.progress.value = 2
        if self.canceled:
            return

        # The spectrum is Hermitian-symmetric, so every peak has a mate
        # mirrored through the DC bin (n // 2 after fftshift). Peaks close
        # enough to the center that their own window already covers the
        # mate are not mirrored again.
        if include_friedel_mates:
            mates = []
            for peak in peaks:
                mate = [2 * c - p for c, p in zip(dc, peak)]
                distance = np.sqrt(sum((m - p)**2
                                       for m, p in zip(mate, peak)))
                if distance > radius:
                    mates.append(mate)
            peaks = peaks + mates

        self.progress.message = 'Applying windows'
        grids = np.meshgrid(*(np.arange(n, dtype=np.float64) for n in shape),
                            indexing='ij')
        window = np.zeros(shape)
        for peak in peaks:
            dist = np.sqrt(sum((g - c)**2 for g, c in zip(grids, peak)))
            window += 0.5 * (erf((dist + radius) / (np.sqrt(2) * sigma)) -
                             erf((dist - radius) / (np.sqrt(2) * sigma)))
        # Overlapping windows must not amplify the spectrum
        np.clip(window, 0.0, 1.0, out=window)
        self.progress.value = 3
        if self.canceled:
            return

        self.progress.message = 'Inverse transform'
        result = np.real(np.fft.ifftn(np.fft.ifftshift(spectrum * window)))
        dataset.active_scalars = np.asfortranarray(result.astype(np.float32))
        self.progress.value = 4
        return {'volume': dataset}


def _detect_peaks(magnitude, dc, threshold, exclude_center_radius,
                  min_peak_size, max_peaks, dedupe_friedel):
    """Peak centers from the thresholded, normalized log spectrum.

    Returns the strongest voxel of each connected region above the
    threshold, strongest peak first. The normalization matches the
    "Fast Fourier Transform (FFT)" operator, so a threshold tuned
    visually on that output applies here unchanged.
    """
    import numpy as np
    from scipy import ndimage

    log_magnitude = np.log(magnitude + np.finfo(float).eps)
    peak = np.max(log_magnitude)
    if peak > 0:
        log_magnitude /= peak
    else:
        # Every |F| below 1 (tiny values in a small volume): dividing by
        # a negative maximum would invert the scale, so stretch to 0..1
        # instead, which keeps the DC term at 1 as the display does.
        low = np.min(log_magnitude)
        log_magnitude = (log_magnitude - low) / (peak - low or 1.0)

    labels, count = ndimage.label(log_magnitude > threshold)
    if count == 0:
        return []
    index = np.arange(1, count + 1)
    sizes = ndimage.sum(np.ones(labels.shape, dtype=np.uint8), labels, index)
    positions = ndimage.maximum_position(magnitude, labels, index)

    candidates = []
    for position, size in zip(positions, sizes):
        if size < max(1, min_peak_size):
            continue
        distance = np.sqrt(sum((p - c)**2 for p, c in zip(position, dc)))
        if distance <= exclude_center_radius:
            continue
        candidates.append((float(magnitude[position]),
                           [float(p) for p in position]))
    candidates.sort(key=lambda item: -item[0])

    peaks = []
    for _, position in candidates:
        if dedupe_friedel:
            # The mate is added back later, so keep one of each pair
            mate = [2 * c - p for c, p in zip(dc, position)]
            if any(np.sqrt(sum((m - q)**2 for m, q in zip(mate, kept)))
                   <= 1.5 for kept in peaks):
                continue
        peaks.append(position)
        if max_peaks > 0 and len(peaks) >= max_peaks:
            break
    return peaks


def _parse_centers(centers, centers_file):
    """Peak triplets from the text parameter and/or the CSV file."""
    rows = []
    if centers:
        for chunk in centers.replace(';', '\n').splitlines():
            rows.append(chunk)
    if centers_file:
        with open(centers_file, 'r', encoding='utf-8-sig',
                  errors='replace') as rf:
            rows.extend(rf.read().splitlines())

    peaks = []
    for row in rows:
        fields = row.replace(',', ' ').split()
        if len(fields) < 3:
            continue
        try:
            peaks.append([float(f) for f in fields[:3]])
        except ValueError:
            # Header or comment line
            continue
    return peaks
